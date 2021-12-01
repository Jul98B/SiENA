#ifndef BATTERY_H_
#define BATTERY_H_

#include "ns3/Device.h"
#include "ns3/Random.h"

#define DEV_FULL 2

namespace ns3 {

class Battery : public Device {

protected:
	double capacity; //wie viel Kapazität hat der Speicher
	double power; //Ladeleistung
	double charge; //Energie im Speicher drin
	double lastCharge; //im vorheriger Tick an Energie im Speicher drin
	double minCharge; //gesetzt als minimaler Ladefüllstand
	double desiredCharge; //gewünschter
	double chargeRateInterval; //länge des Ticks mit berücksichtigen
	int chargeStatId;
	int pChargeStatId;
	double chargeRateH;
	double efficiency; // 0,95 evtl, verluste berücksichtigen

public:
	Battery(std::string id);
	virtual ~Battery() {};
	virtual void init(Json::Value config);
	virtual void tick();
	virtual double getConsumption();
	double getCharge() { return charge; }
	double getLastCharge() { return lastCharge; }
	double getCapacity() { return capacity; }
	virtual bool isAdaptable();
	virtual std::pair<AdaptionFlex*, AdaptionOnOff*> getAdaption();
	double getPower() { return power; };
	double getEfficiency() { return efficiency; };
//	virtual void setAdaptedConsumption(double consumption);

};

}

#endif
