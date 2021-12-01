#include "ns3/core-module.h"
#include "ns3/siena-module.h"
#include <string>
#include <ctime>
#include "ns3/MyStats.h"
#include "ns3/StatsExporter.h"
#include "ns3/StatsAnalyzer.h"
#include "ns3/Rscript.h"
#include "ns3/MyConfig.h"
#include "ns3/Tick.h"
#include "ns3/Registry.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/TraceHelper.h"
#include "ns3/plc-defs.h"


#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char* argv[]) {
	try {
		time_t start = time(NULL);

		Config::SetDefault("ns3::ArpCache::PendingQueueSize", UintegerValue(1000));
		Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (1024 * 100));
//		CommandLine cmd;
//		cmd.Parse(argc, argv);


//		LogComponentEnableAll(LOG_PREFIX_ALL);
//		LogComponentEnableAll(LOG_LEVEL_ALL);
//		LogComponentEnable("PLC_Mac", LOG_LEVEL_ALL);
//		LogComponentEnable("PLC_Phy", LOG_LEVEL_ALL);

		Packet::EnablePrinting(); // drin lassen!

		Time::SetResolution(Time::NS);
		MyConfig* config = MyConfig::Get();
		config->init(argc, argv);
		Random::Get()->init(config->getInt("seed"));

//		DataBasis::Get()->convertFilesToBinary();

		registerModules(); //alle Fubktionspointer zu Stichpunkten in Liste schreiben 

		SiENASimulator sim; //in dem Konstruktor wird ganze Simulation mit Cluste und Homeconfig erstellt
		time_t initEnd = time(NULL);

//		PointToPointHelper p2p;
//		p2p.EnablePcapAll("test");
//		FlowMonitorHelper flowmon;
//		Ptr<FlowMonitor> monitor = flowmon.InstallAll();

		//Log::f("Simulator", "starting simulation...");

		Simulator::Run();

		/*
		//Hier aus log-Datei lesen und über MQTT schicken?
		std::string filename1("gridhomes_config.txt");
		std::string s;
		std::string payload = "Config: \\n";

		std::ifstream input_file1(filename1);
		if (!input_file1.is_open()) {
			std::cerr << "Could not open the file - '"
					<< filename1 << "'" << std::endl;

			return EXIT_FAILURE;
		}

		while (input_file1 >> s) {
			payload += s;
		}

		std::cout << "Payload config: " << std::endl;
		std::cout << payload << std::endl;

		std::cout << std::endl;
		input_file1.close();

		payload += "Log: \\n";

		//hier log auslesen und an Payload anhaengen:
		std::string filename2("gridhome_setAdaption.txt");
		std::ifstream input_file2(filename2);
		if (!input_file2.is_open()) {
			std::cerr << "Could not open the file - '"
					<< filename2 << "'" << std::endl;

			return EXIT_FAILURE;
		}

		while (input_file2 >> s) {
			payload += s;
		}

		std::cout << "Payload config + log: " << std::endl;
		std::cout << payload << std::endl;

		std::cout << std::endl;
		input_file2.close();

		//HIER MQTT CLIENT STARTEN UND PAYLOAD MIT QoS 0 (weniger Overhead) SCHICKEN
		//... keine Ahnung wie...   */


		//Log::f("Julie :D ", "end simulation... !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

//		monitor->CheckForLostPackets ();
//		std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
//		std::map<FlowId, FlowMonitor::FlowStats>::iterator it;
//		double rx = 0;
//		for(it = stats.begin(); it != stats.end(); ++it) {
//			rx += it->second.rxBytes;
//		}
//		double throughput = (double) rx / stats.rbegin()->second.timeLastRxPacket.GetSeconds() / 1024.0;
//		std::cout << "sent: " << rx / 1024 / 1024 << " MB" << std::endl;
//		std::cout << "avg throughput: " << throughput << " kB/s" << std::endl;
//		std::cout << rx << std::endl;

		TraceHelper::Get()->print();

		Modules::Get()->execute(config->getString("tidy_up"), NULL);

		Simulator::Destroy();
		time_t simEnd = time(NULL);

		MyStats::Get()->exportCsv();
		StatsExporter::Get()->exportAdditionalStatsToCsv();
		if(config->getBool("analyze"))
			StatsAnalyzer::analyze();
		time_t statsEnd = time(NULL);

		Log::f("Simulator", "creating graphs...");
		Rscript::runRscripts();

		time_t graphsEnd = time(NULL);

		double initTime = difftime(initEnd, start);
		double simTime = difftime(simEnd, initEnd);
		double statsTime = difftime(statsEnd, simEnd);
		double graphsTime = difftime(graphsEnd, statsEnd);
		double totalTime = difftime(graphsEnd, start);
		std::cout << std::endl << "simulation finished in " << Tick::getTime(totalTime)
			<< " (init: " << Tick::getTime(initTime)
			<< ", simulation: " << Tick::getTime(simTime)
			<< ", stats: " << Tick::getTime(statsTime)
			<< ", graphs: " << Tick::getTime(graphsTime) << ")" << std::endl;
	} catch(const std::string& s) {
		std::cerr << s << std::endl;
	} catch(const char* s) {
		std::cerr << s << std::endl;
	}
	return 0;
}
