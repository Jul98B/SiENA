#include "../../siena/home/GridHome.h"
#include "../../siena/devices/Car.h" //fuer Car
#include "../../siena/devices/Battery.h" //fuer Car


#include <iostream>
#include <fstream>
#include <string>

#include <mosquitto.h>


#define LOG
#define MQTT 

namespace ns3 {

TypeId GridHome::GetTypeId() {
	static TypeId id = TypeId("ns3::GridHome")
		.SetParent<Application>()
		.AddConstructor<GridHome>()
		.AddAttribute("id", "id", StringValue("gridhome"), MakeStringAccessor(&GridHome::id), MakeStringChecker());
	return id;
}

GridHome::GridHome() : ConventionalHome("error"), token(NULL), lastAdaption(-1) {
	communicator = Communicator::Get();
	algo = Modules::Get();
	moduleShift = c->getString("gridhome_shift_device");
	moduleToken = c->getString("gridhome_handle_token");
	moduleTick = c->getString("gridhome_tick");
	suppressTick = c->getBool("suppress_auto_home_tick");
	traceHelper = TraceHelper::Get();
	packetLog = PacketLog::Get();
	packetLogger = PacketLogger::Get();

	#ifdef MQTT
		//MQTT verbindung erstellen:
		mosquitto_lib_init();

		//client id, 
		mosq = mosquitto_new(NULL, true, NULL);

		//mosquitto_username_pw_set(mosq,"name","PW");
		//rc = mosquitto_connect(mosq, "IP", Port, 60);

		rc = mosquitto_connect(mosq, "localhost", 1883, 60);
		if(rc != 0){
			printf("Client could not connect to broker! Error Code: %d\n", rc);
			mosquitto_destroy(mosq);
			return;
		}
		std::cout << "We are now connected to the broker!\n" << std::endl;
	#endif
}

void GridHome::StartApplication() {
	#ifndef TURBO
	Log::i(id, "starting application...");
	#endif
	TypeId id = TypeId::LookupByName(communicator->getSocketFactory());
	listenerSocket = Socket::CreateSocket(GetNode(), id);
	listenerSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 80));
	listenerSocket->Listen();
	listenerSocket->ShutdownSend();
	listenerSocket->SetRecvCallback(MakeCallback(&GridHome::handleRead, this));
	listenerSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &> (), MakeCallback (&GridHome::handleAccept, this));

	ip = GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
	communicator->registerRecipient(this, Helper::toString(ip));
}

void GridHome::StopApplication() {
	listenerSocket->Close();
	//verbindung trennen und alles wieder aufraeumen
	mosquitto_disconnect(mosq);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
}

void GridHome::tick() {
	if(t->getTick() > simEnd) // catch late packets after sim end
		return;
	ConventionalHome::tick();

	std::vector<void*> params;
	params.push_back(this);
	algo->execute(moduleTick, &params);
}

void GridHome::schedule(DeviceEvent* event) {
	std::vector<void*> params;
	params.push_back(this);
	params.push_back(event);
	params.push_back(token);
	algo->execute(moduleShift, &params);
}

void GridHome::scheduleSend(std::string destination, Ptr<Packet> packet) {
	Simulator::Schedule(Seconds(0), &GridHome::send, this, destination, packet);
}

void GridHome::scheduleSend(Time t, std::string destination, Ptr<Packet> packet) {
	Simulator::Schedule(t, &GridHome::send, this, destination, packet);
}

void GridHome::send(std::string destination, Ptr<Packet> packet) {
	#ifndef	TURBO
		std::stringstream s;
		s << "Sending Packet to " << destination << " (Uid: " << packet->GetUid() << ")";
		Log::i(id, s.str());
	#endif
    traceHelper->addTag(packet, ip, destination);
    packetLogger->logPacketTx(packet);
	this->communicator->send(destination, packet, this->GetNode());
}

void GridHome::handleAccept(Ptr<Socket> socket, const Address& address) {
	socket->SetRecvCallback(MakeCallback(&GridHome::handleRead, this));
}

void GridHome::handleRead(Ptr<Socket> socket) { 
	
	Ptr<Packet> packet;
	Address sender;
	while((packet = socket->RecvFrom(sender))) {
		if(packetLog->check(packet))
			handlePacket(packet); 
		#ifndef TURBO
		else
			Log::i(id, "received duplicate packet, discarding, uid: ", packet->GetUid());
		#endif
	}
}

void GridHome::handlePacket(Ptr<Packet> packet) {
	Log::i(id, "received packet, uid: ", packet->GetUid());
	if(t->getTick() > simEnd) // catch late packets after sim end
		return;

	// handle packet
	packetLogger->logPacketRx(packet);
	token = Token::extractToken(packet);

	// tick
	this->tick();

	// adaption
	setAdaption();

	// handle token
	std::vector<void*> params;
	params.push_back(this);
	params.push_back(token);
	algo->execute(moduleToken, &params); //hier wird der Priv algo gestartet: Stichwort= gridhome_handle_token
}

Adaption* GridHome::getAdaption() {
	setAdaption();
	return &adaption;
}

void GridHome::setAdaption() {

	if(t->getTick() > lastAdaption) {

		#ifdef LOG 
			std::string filename("gridhome_setAdaption.txt"); //in diese Textdatei alle Logs schreiben
			std::ofstream file_out;

			file_out.open(filename, std::ios_base::app); 

			file_out << "{ \"gridhome\":{\"ipAdresse\":\"" << this->getIp() << "\",";
			file_out << "\"time\":" << t->getTick() << "},"; //zahlt in ganzen Zahlen die Zeitslots
		#endif

		#ifdef MQTT 
			std::string payload = "";
			payload += "{\"gridhome\":{\"ipAdresse\":\"";
			payload +=  std::to_string(this->getIp().Get());
			payload += "\",\"time\":" + std::to_string(t->getTick()) + "},";
		#endif

		Log::i(id, "\tadaption:");
		adaption = Adaption();
		double minimum = 0, maximum = 0, desired = 0, base = 0;
		std::map<std::string, Device*>::iterator it;
		for(it = devices.begin(); it != devices.end(); ++it) { //durch Liste aller Devices dieses Gridhomes itterieren
			
			//#ifdef LOG || MQTT
			//devicetype aus der kompletten id holen und ende Abschneiden (angehängte Zahlen bleiben bestehen)
			std::string id = it->second->getId();
			std::string deviceType = id.substr(13, id.length()); //Home und Clusternummer abschneiden 

			if(deviceType[0] == '_'){ //zweistellige GridHome Nummer: einen mehr abschneiden
				deviceType = deviceType.substr(1,deviceType.length());
			} else if(isdigit(deviceType[0])){ //dreistellige Gridhome Nummer: zwei mehr abschneiden
				deviceType = deviceType.substr(2,deviceType.length());
			}

			//Trennzeichen bei Nummerierung tauschen von ":" zu "_"
			std::size_t pos = deviceType.find(":", 0);

			if(pos != std::string::npos){ //wenn gefunden
				deviceType[pos] = '_';
			}

			//#endif

			#ifdef LOG 
				file_out << "\"" << deviceType << "\":{";
			#endif
			#ifdef MQTT
				payload += "\"" + deviceType + "\":{";
			#endif

			if(!(std::string(typeid(*it->second).name())).compare("N3ns33CarE")){ //Auto: fuel und Charge extra dazu schreiben
				#ifdef LOG 
					file_out << "\"fuel\":" << std::to_string(dynamic_cast<Car*>(it->second)->getFuelEconomy());
					file_out << ",\"charge\":" << std::to_string(dynamic_cast<Battery*>(it->second)->getCharge()) << ",";
				#endif

				#ifdef MQTT
					payload += "\"fuel\":" + std::to_string(dynamic_cast<Car*>(it->second)->getFuelEconomy());
					payload += ",\"charge\":" + std::to_string(dynamic_cast<Battery*>(it->second)->getCharge()) + ",";
				#endif
			} 

			//moegliche Energieverschiebung und aktuellen Verbrauch fuer neuen Zeitpunkt setzen
			if(it->second->isAdaptable()) {
				std::pair<AdaptionFlex*, AdaptionOnOff*> a = it->second->getAdaption();
				if(a.first->isAdaptable()) {
					minimum += a.first->getMinimum(); //bei Auto evtl veraendern: weniger als Batterie nutzen
					maximum += a.first->getMaximum();
					desired += a.first->getDesired();
					#ifdef LOG 
						file_out << "\"status\":\"adaptionFlex\",\"min\":" << minimum << ",\"max\":" << maximum << ",\"desired\":" << desired << "},"; // Min, Max, Desired: Flexibel und adaptable
					#endif
					#ifdef MQTT
						payload += "\"status\":\"adaptionFlex\",\"min\":" + std::to_string(minimum) + ",\"max\":" + std::to_string(maximum) + ",\"desired\":" + std::to_string(desired) + "},";
					#endif
				} else if(a.second != NULL) {
					adaption.addOnOff(a.second);
					base += a.second->getCurrent();
					#ifdef LOG 
						file_out << "\"status\":\"adaptionOnOff\",\"current\":" << a.second->getCurrent() << "},";// Current: nur bei On/Off und adaptable
					#endif
					#ifdef MQTT
						payload += "\"status\":\"adaptionOnOff\",\"current\":" + std::to_string(a.second->getCurrent()) + "},";
					#endif
				}
				#ifndef TURBO
					if(a.second != NULL)
						Log::i(id, "\t\t" + it->first + " on/off: " + a.second->toString());
					if(a.first->isAdaptable())
						Log::i(id, "\t\t" + it->first + " flex: " + a.first->toString());
				#else
					Log::i(id, "\toptimized out");
				#endif
			} else {
				base += it->second->getConsumption();
				#ifdef LOG 
					file_out << "\"status\":\"notAdaptable\",\"consumption\":" << it->second->getConsumption() << "},";
				#endif
				#ifdef MQTT
					payload += "\"status\":\"notAdaptable\",\"consumption\":" + std::to_string(it->second->getConsumption()) + "},";
				#endif
			}
			
			}
			adaption.setFlex(AdaptionFlex(minimum, maximum, desired, INT_MIN, desired));
			adaption.setBase(base);
			#ifndef TURBO
				if(adaption.getFlex()->isAdaptable())
					Log::i(id, "\t\t-> total flex: " + adaption.getFlex()->toString());
				Log::i(id, "\t\t-> base: ", base);
			#endif
			lastAdaption = t->getTick();

			#ifdef LOG 
				file_out << "\"home_total\":{\"min\":" << minimum << ",\"max\":" << maximum << ",\"desired\":" << desired << ",\"base\":" << base << "}}\n";
			#endif
			
			#ifdef MQTT
				payload += "\"home_total\":{\"min\":" + std::to_string(minimum) + ",\"max\":" + std::to_string(maximum) + ",\"desired\":" + std::to_string(desired) + ",\"base\":" + std::to_string(base) + "}}";
			#endif
		
			#ifdef MQTT
				
				if(rc == 0){
					//string zu char* machen
					const char* c = payload.c_str();
					std::string topic = "/sienahome/" + std::to_string(this->getIp().Get());
					//std::string topic = "test";
					mosquitto_publish(mosq, NULL, topic.c_str(), payload.length(), c, 0, false); 
					//std::cout << "müsste verschickt sein o.O unter topic: " << topic << std::endl;
				}
				//std::cout << "Payload: " << payload << std::endl; //in der Konsole zum direkt pruefen

				//payload = ""; //leer machen fuer naechsten Durchlauf
			#endif
			
		}
	}
}