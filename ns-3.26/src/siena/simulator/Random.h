#ifndef RANDOM_H_
#define RANDOM_H_

#include <map>
#include <string>
#include <cstdlib>
#include "ns3/MySingleton.h"
#include <algorithm>
#include "ns3/MyConfig.h"
#include <iostream>

namespace ns3 {

class RandomForce {
private:
	double counter;
	double target, value;
public:
	RandomForce(double value) : counter(1/(2*value)), target(1/value), value(1/value) {}
	bool next() { // value groß: haeufiger false, value klein: haeufiger true 
		/*std::cout << "next in Randomforce: " << std::endl;
		//anfangs sind Value und Target immer gleich?!?
		std::cout << "value: " << std::to_string(value) << std::endl; 
		std::cout << "target: " << std::to_string(target) << std::endl;
		std::cout << "counter: " << std::to_string(counter) << std::endl;*/
		

		if(++counter > target) { //counter hochzaehlen fuer naechsten Aufruf
			target += value; //Target nach oben korrigieren, damit naechster Aufruf nicht automatisch if(true) ist
			/*
			std::cout << "if target: " << std::to_string(target) << std::endl;
			std::cout << "if counter: " << std::to_string(counter) << std::endl;
			*/
			return true;
		}
		return false;
	}
};

class Random : public MySingleton<Random> {
	friend class MySingleton<Random>;

private:
	std::map<std::string, std::vector<double>*> randomLists;
	std::map<std::string, RandomForce*> forceList;
	MyConfig* config;
	bool forceOwn;

public:
	virtual ~Random() {
		std::map<std::string, std::vector<double>*>::iterator it;
		for(it = randomLists.begin(); it != randomLists.end(); ++it) {
			delete it->second;
		}
		std::map<std::string, RandomForce*>::iterator jt;
		for(jt = forceList.begin(); jt != forceList.end(); ++jt) {
			delete jt->second;
		}
	}

	void init(int seed) {
		srand(seed);
	}

	void generateList(std::string name, int length) {
		std::vector<double>* list = new std::vector<double>;
		list->push_back(1);
		for(int i = 1; i <= length; i++) {
			list->push_back(getD());
		}
		randomLists.insert(std::pair<std::string, std::vector<double>*>(name, list));
	}

	int get() { return rand(); }
	double getD() { return (double) rand() / (double) RAND_MAX; }

	int get(int lower, int upper) {
		return lower + getD() * (upper - lower);
	}

	std::vector<double>* get(std::string name) {
		std::map<std::string, std::vector<double>*>::iterator it = randomLists.find(name);
		if(it != randomLists.end())
			return it->second;
		throw("random list " + name + " does not exist");
	}

	double getNext(std::string name) {
		std::vector<double>* list = get(name);
		uint index = (*list)[0];
		if(index > list->size())
			throw("random list " + name + " overflow");
		(*list)[0] = index + 1;
		return (*list)[index];
	}

	std::vector<int> randomList(int start, int end) {
		std::vector<int> list;
		for(int i = start; i < end; i++) {
			list.push_back(i);
		}
		std::random_shuffle(list.begin(), list.end());
		return list;
	}

	std::vector<double> getDoubleList(int n) {
		std::vector<double> list(n, 0);
		for(int i = 0; i < n; i++) {
			list[i] = getD();
		}
		return list;
	}

	std::vector<int> getIntList(int n) {
		std::vector<int> list(n, 0);
		for(int i = 0; i < n; i++) {
			list[i] = get();
		}
		return list;
	}

	bool getDev(std::string dev) {
		//std::cout << "Random: getDev " << dev << std::endl;

		double own = config->getDouble("own_" + dev);
		//std::cout << "own: " << std::to_string(own) << std::endl;

		if(own > 1)
			own -= (int) own;

		if(forceOwn) {
			//std::cout << "forceown" << std::endl;

			//random Double der nicht genutzt wird? 
			getD(); // dummy, keep

			if(own == 0)
				return false; //wird nicht hinzugefuegt weil laut config wahrscheinlichkeit = 0

			std::map<std::string, RandomForce*>::iterator it = forceList.find(dev);

			if(it == forceList.end()) //device war noch nicht in der Liste
				it = forceList.insert(std::pair<std::string, RandomForce*>(dev, new RandomForce(own))).first; //own ist Value in RandomForce
			return it->second->next();
		} else {
			//wird gerade nicht ausgefuehrt
			if(getD() <= own) 
				return true;
			return false;
		}
	}

private:
	Random() {
		config = MyConfig::Get();
		forceOwn = config->getBool("force_own");
	}

};

}

#endif
