#include<iostream>
#include<fstream>
#include<string>
#include<iomanip>
#include<vector>
#include<queue>

#include "App.h"
//#include "Scheduler.h"

#define BWmax 15.75
//#define BWmax 10

using namespace std;

bool getInput();
bool simColocation();

int Napp, Ialg, Mode, verb;
double MAX = 100;
vector<App> Apps;

int main() {
	if(!getInput()) {
		cout << "Error from reading input\n";
		return -1;
	}

	if(!simColocation()) {
		cout << "Error from simulating colocation\n";
		return -1;
	}
}

bool simColocation() {
	int i, j, count[4] = {0}, loop[Napp], ac[Napp];
	double k_sch = 0, now, rt[Napp], bw[4] = {0};
	priority_queue<Event> evtq;

	for(i=0; i<Napp; i++) {
		Kernel* cur = Apps[i].getIteration()->getFirstKernel();
		Event newEvt;
		newEvt.setKernel(cur);
		newEvt.setTime(cur->getStart());
		newEvt.setStatus(1);
		evtq.push(newEvt);
		ac[i] = 1;
		loop[i] = MAX / Apps[i].getLength();
		if(loop[i] < 1) loop[i] = 1;
if(verb >= 1) cout << "App " << i << " have " << loop[i] <<  " loops.\n";
	}

	while( !(evtq.empty()) ) {
/*		for(i=0; i<Napp; i++) { 
			int size = cur[i]->getSizeNextKernels();
			for(j=0; j<size; j++) next.push_back(cur[i]->getNextKernel(j));
		} */
		now = evtq.top().getTime();
if(verb >= 1) cout << "At " << now << ": " << evtq.size() << " events are in progress\n";
		switch(evtq.top().getStatus()) {
case 1: // Submitted
			if(evtq.top().getType() == 1) {
//GPUconf
				if(count[1] == 0) {
					count[1]++;
					Event newEvt;
					newEvt.setEvent(evtq.top());
					newEvt.setStatus(2);
					newEvt.setTime(now + evtq.top().getLength());
					k_sch = newEvt.getTime();
					evtq.pop();
					evtq.push(newEvt);

if(verb >= 1) {
	cout << "New kernel ";
	if(verb >= 2) cout << "[From " << now << " to " << newEvt.getTime() << "] ";
	cout << "started.\n";
}

				}
				else {
					Event newEvt;
					newEvt.setEvent(evtq.top());
					newEvt.setTime(evtq.top().getTime() + (k_sch - now) + 0.00001);
					evtq.pop();
					evtq.push(newEvt);

if(verb >= 1) {
	cout << "Another kernel is already scheduled... ";
	if(verb >= 2) cout << "[Wait until " << newEvt.getTime() << "]";
	cout << endl;
if(now == newEvt.getTime()) return false;
}

				}
			}
			else {
				count[evtq.top().getType()]++;
				bw[evtq.top().getType()] += evtq.top().getBW();
				Event newEvt;
				newEvt.setEvent(evtq.top());
				newEvt.setStatus(2);
//BWconf
				newEvt.setTime(now + evtq.top().getLength() * count[evtq.top().getType()]);
/*				if(bw[evtq.top().getType()] > BWmax) 
					newEvt.setTime(now + evtq.top().getLength() * bw[evtq.top().getType()]);
				else
					newEvt.setTime(now + evtq.top().getLength()); */

				evtq.pop();
if(verb >= 1) {
	cout << "New memcpy ";
	if(verb >= 2) {
		cout << "[From " << now << " to " << newEvt.getTime() << "] ";
		if(bw[evtq.top().getType()] > BWmax) cout << "(with Contention) ";
		else if (count[evtq.top().getType()] > 1) cout << "(Without Contention) ";
		else cout << "(Solo) ";
	}
	cout << "started.\n";
}

//BWconf
				if(count[newEvt.getType()] == 1) evtq.push(newEvt);
//				if(bw[newEvt.getType()] <= BWmax) evtq.push(newEvt);
				else {

if(verb >= 1) {
	cout << "There are co-scheduled memcpys ";
	if(newEvt.getType() == 2) cout << "(H->D), ";
	else cout << "(D->H), ";
	cout << "and their finish time is delayed.\n";
}

					priority_queue<Event> evtq2;
					evtq2.push(newEvt);
					while(!evtq.empty()) {
						if(evtq.top().getType() == newEvt.getType() && evtq.top().getStatus() == 2) {
							Event newEvt2;
							newEvt2.setEvent(evtq.top());
//BWconf
if(verb >= 2) cout << "Finish time of existing memcpy delayed from " << evtq.top().getTime();
							newEvt2.setTime( (evtq.top().getTime() - now) * count[newEvt.getType()] / (count[newEvt.getType()] - 1) + now);
/*							if(bw[newEvt.getType()] - newEvt.getBW() <= BWmax)
								newEvt2.setTime( (evtq.top().getTime() - now) * bw[newEvt.getType()] / BWmax + now);
							else
								newEvt2.setTime( (evtq.top().getTime() - now) * bw[newEvt.getType()] / (bw[newEvt.getType()] - newEvt.getBW()) + now);
*/
if(verb >= 2) cout << " to " << newEvt2.getTime() << endl;
							evtq.pop();
							evtq2.push(newEvt2);
						}
						else {
							evtq2.push(evtq.top());
							evtq.pop();
						}
					}

					while(!evtq2.empty()) {
						evtq.push(evtq2.top());
						evtq2.pop();
					}
				}
			}
			break;
case 2: // Scheduled
			int aid = evtq.top().getKernel()->getAppId();
			ac[aid]--;
			if(evtq.top().getType() == 1) {
if(verb >= 1) cout << "Scheduled kernel is finished.\n";
				count[1]--;
				k_sch = 0;
				Kernel* k = evtq.top().getKernel();
				evtq.pop();

				int size = k->getSizeNextKernels();
				for(i=0; i<size; i++) {
					Kernel* cur = k->getNextKernel(i);
					Event newEvt;
					newEvt.setKernel(cur);
					newEvt.setTime( now + (cur->getStart() - k->getEnd()) * (1 + Apps[aid].getCPUslow()) );
					newEvt.setStatus(1);
					evtq.push(newEvt);
					ac[cur->getAppId()]++;

if(verb >= 2) cout << "New event will start at " << newEvt.getTime() << endl;

				}

if(size > 1 && verb == 1) cout << "New events are registered.\n";
if(size == 1 && verb == 1) cout << "New event is registered.\n";

			}
			else {
if(verb >= 1) cout << "Scheduled memcpy is finished.\n";
				int type = evtq.top().getType();
				double tbw = evtq.top().getBW();
				count[type]--;
				bw[type] -= tbw;
				Kernel* k = evtq.top().getKernel();
				evtq.pop();

				int size = k->getSizeNextKernels();
				for(i=0; i<size; i++) {
					Kernel* cur = k->getNextKernel(i);
					Event newEvt;
					newEvt.setKernel(cur);
					newEvt.setTime( now + (cur->getStart() - k->getEnd()) * (1 + Apps[cur->getAppId()].getCPUslow()) );
					newEvt.setStatus(1);
					evtq.push(newEvt);
					ac[cur->getAppId()]++;

if(verb >= 2) cout << "New event will start at " << newEvt.getTime() << endl;

				}

if(size > 1 && verb == 1) cout << "New events are registered.\n";
if(size == 1 && verb == 1) cout << "New event is registered.\n";

//BWconf
				if(count[type] >= 1) {
//				if(bw[type] + tbw > BWmax) {

if(verb >= 1) {
	cout << "The number of co-scheduled memcpys ";
	if(type == 2) cout << "(H->D), ";
	else cout << "(D->H), ";
	cout << "is reduced, so their finish time is also reduced\n";
}

					priority_queue<Event> evtq2;
					while(!evtq.empty()) {
						if(evtq.top().getType() == type && evtq.top().getStatus() == 2) {
							Event newEvt2;
							newEvt2.setEvent(evtq.top());
//BWconf
//if(verb >= 2) cout << "Finish time of existing memcpy has been brought forward from " << evtq.top().getTime();
							newEvt2.setTime( (evtq.top().getTime() - now) * count[type] / (count[type] + 1) + now);
//							if(bw[type] > BWmax)
//								newEvt2.setTime( (evtq.top().getTime() - now) * bw[type] / (bw[type] + tbw) + now);
//							else 
//								newEvt2.setTime( (evtq.top().getTime() - now) * bw[type] / BWmax + now);

if(verb >= 2) cout << " to " << newEvt2.getTime() << endl;
							evtq.pop();
							evtq2.push(newEvt2);
						}
						else {
							evtq2.push(evtq.top());
							evtq.pop();
						}
					}

					while(!evtq2.empty()) {
						evtq.push(evtq2.top());
						evtq2.pop();
					}
				}
			}

			if(ac[aid] == 0) {
				if(loop[aid] > 1) {
					Kernel* cur = Apps[aid].getIteration()->getFirstKernel();
					Event newEvt;
					newEvt.setKernel(cur);
					newEvt.setTime(cur->getStart() + now);
//					newEvt.setTime(Apps[aid].getLength() - cur->getStart() + now);
					newEvt.setStatus(1);
					evtq.push(newEvt);
					loop[aid]--;
					ac[aid]++;
if(verb >= 1) cout << "App " << aid << " started new loop (" << loop[aid] << " remained).\n";
				}
				else rt[aid] = now;
			}

			break;
		}

	}

	cout << "            App           Loop      O.Runtime      R.Runtime       Slowdown\n";
	for(i=0; i<Napp; i++) {
		int l = MAX / Apps[i].getLength();
		if(l < 1) l = 1;
		cout << setw(15) << i << setw(15) << l << setw(15) << l * Apps[i].getLength() << setw(15) << rt[i] << setw(14) << (rt[i] / (l * Apps[i].getLength()) - 1) * 100 << "%" << endl;
	}
	cout << "Simulation ends at " << now << endl;
	return true;
}

bool getInput() {
	ifstream conf, appin;
	string name, temp;
	double value;
	int i, j;

	conf.open("Conf.dat");
	cout << "Open configuration file.\n";

	conf >> Mode;
	conf >> Ialg;
	conf >> MAX;
	conf >> verb;
	conf >> Napp;

	for(i=0; i<Napp; i++) {
		conf >> name;
		appin.open(name.c_str());
		cout << "Open Application data for App " << i << ".\n";

		App newApp;
		newApp.setName(temp);

		conf >> value;
		newApp.setCPUslow(value);

		appin >> temp >> value;
		newApp.setLength(value);
		appin >> temp >> value;
		newApp.setLengthGPU(value);
		appin >> temp >> value;
		newApp.setLengthCPU(value);

		Kernel* head = new Kernel(i,1,0,0,0,0);
		newApp.getIteration()->setFirstKernel(head);
		newApp.getIteration()->setLastKernel(head);

		while(true) {
			Kernel* newKernel = new Kernel();
			appin >> value;
			if(appin.eof()) break;
			newKernel->setStream(value);
			newKernel->setAppId(i);

			appin >> temp;
			if(temp == "[CUDA_memcpy_HtoD]") newKernel->setType(2);
			else if(temp == "[CUDA_memcpy_DtoH]") newKernel->setType(3);
			else if(temp == "[CUDA_memset]") {
				delete newKernel;
				appin >> value >> value >> value;
				continue;
			}
			else newKernel->setType(1);

			appin >> value;
			newKernel->setStart(value);
			appin >> value;
			newKernel->setLength(value);
			appin >> value;
			newKernel->setBW(value);

if(verb == 3) {
	cout << "Read new event: ";
	if(newKernel->getType() == 1) cout << "Kernel ";
	else if(newKernel->getType() == 2) cout << "Memcpy(H->D) ";
	else cout << "Memcpy(D->H) ";
	cout << "Starts at " << newKernel->getStart() << " and length is " << newKernel->getLength() << endl;
}
	
			if(newApp.getIteration()->getFirstKernel() == NULL) {
				newApp.getIteration()->setFirstKernel(newKernel);
				newApp.getIteration()->setLastKernel(newKernel);
			}
			else {
				Kernel* last = newApp.getIteration()->getLastKernel();
				if(last->getEnd() <= newKernel->getStart()) {
					newKernel->setParent(last);
					if(last->addNextKernel(newKernel) == false) {
						cout << "ERROR: data have problem - invalid type[0]\n";
						return false;
					}
					newApp.getIteration()->setLastKernel(newKernel);
					continue;
				}
				Kernel* par = last->getParent();
				int size = par->getSizeNextKernels();
				for(j=size-1; j>=0; j--) {
					Kernel* bro = par->getNextKernel(j);
					if(bro->getEnd() <= newKernel->getStart()) {
						newKernel->setParent(bro);
						if(bro->addNextKernel(newKernel) == false) {
							cout << "ERROR: data have problem - invalid type[1]\n";
							return false;
						}
						newApp.getIteration()->setLastKernel(newKernel);
						break;
					}
				}

				if(j >= 0) continue;
				if(par->getEnd() > newKernel->getStart()) {
					cout << "ERROR: data have problem - invalid time[2]\n";
					return false;
				}

				newKernel->setParent(par);
				if(par->addNextKernel(newKernel) == false) {
					cout << "ERROR: data have problem - invalid time[3]\n";
					delete newKernel;
					continue;
//					return false;
				}
				newApp.getIteration()->setLastKernel(newKernel);
			}
		}

cout << newApp.getLength() << endl;
		Kernel* tail = new Kernel(i,1,0,newApp.getLength(),0,0);
		tail->setParent(newApp.getIteration()->getLastKernel());
		newApp.getIteration()->getLastKernel()->addNextKernel(tail);
		newApp.getIteration()->setLastKernel(tail);

		appin.close();

		Apps.push_back(newApp);
	}

	conf.close();

	cout << "Reading data is done.\n";
	if(Apps[0].getIteration()->getFirstKernel() == NULL) cout << "XXXX\n";
	return true;
}
