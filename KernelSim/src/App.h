#include<vector>
#include<string>

#ifndef APP_H
#define APP_H

using namespace std;

class Kernel {
private:
	int appid;
	int type;
	int stream;
	double start;
	double length;
	double end;
	double bw;
	vector<Kernel*> next;
	Kernel* parent;

	void computeEnd() { end = start + length; }
public:
	Kernel(int a = 0, int t = 0, int s = 0, double st = 0.0, double l = 0.0, double b = 0) {
		appid = a;
		type = t;
		stream = s;
		start = st;
		length = l;
		bw = b;
		computeEnd();
		parent = NULL;
	}

	void setKernel(Kernel k) {
		appid = k.getAppId();
		type = k.getType();
		stream = k.getStream();
		start = k.getStart();
		length = k.getLength();
		computeEnd();
		parent = k.getParent();
	}

	int getAppId() { return appid; }
	int getType() { return type; }
	int getStream() { return stream; }
	double getStart() { return start; }
	double getLength() { return length; }
	double getEnd() { return end; }
	double getBW() { return bw; }
	vector<Kernel*> getNextKernels() { return next; }
	Kernel* getNextKernel(int idx) { return next[idx]; }
	int getSizeNextKernels() { return next.size(); }
	Kernel* getParent() { return parent; }

	void setAppId(int a) { appid = a; }
	void setType(int t) { type = t; }
	void setStream(int s) { stream = s; }
	void setStart(double s) { start = s; computeEnd(); }
	void setLength(double l) { length = l; computeEnd(); }
	void setBW(double b) { bw = b; }
	void setParent(Kernel* k) { parent = k; }
	bool addNextKernel(Kernel* k) { 
		for(int i=0; i<next.size(); i++) if(k->getType() == next[i]->getType()) return false;
		next.push_back(k);
		return true;
	}

	void removeNextKernels() { next.clear(); }
	void removeNextKernel(int idx) { next.erase(next.begin() + idx); }
};

class Iteration {
private:
	Kernel* k_first;
	Kernel* k_last;
	double length;

public:
	Iteration(Kernel* kf = NULL, Kernel* kl = NULL, double l = 0.0) {
		k_first = kf;
		k_last = kl;
		length = l;
	}

	Kernel* getFirstKernel() { return k_first; }
	Kernel* getLastKernel() { return k_last; }
	double getLength() { return length; }

	void setFirstKernel(Kernel* k) { k_first = k; }
	void setLastKernel(Kernel* k) { k_last = k; }
	void setLength(double l) { length = l; }
};

class App {
private:
	string name;
	Iteration *iter;
	double length, l_cpu, l_gpu;
	double CPUslow;

public:
	App(string n = "nothing", double l = 0) {
		name = n;
		length = l;
		iter = new Iteration();
	}
	string getName() { return name; }
	Iteration* getIteration() { return iter; }
	double getLength() { return length; }
	double getLengthCPU() { return l_cpu; }
	double getLengthGPU() { return l_gpu; }
	double getCPUslow() { return CPUslow; }

	void setName(string n) { name = n; }
	void setIteration(Iteration* i) { iter = i; }
	void setLength(double l) { length = l; }
	void setLengthCPU(double l) { l_cpu = l; }
	void setLengthGPU(double l) { l_gpu = l; }
	void setCPUslow(double s) { CPUslow = s; }
};

class Event {
private:
	int type;
	int stream;
	int status;
	double time;
	double length;
	double bw;
	Kernel* kernel;

public:
	Event(int ty = 0, int str = 0, int sta = 0, double ti = 0, double l = 0, double f = 0, Kernel* k = NULL) {
		type = ty;
		stream = str;
		status = sta;
		time = ti;
		length = l;
		bw = f;
		kernel = k;
	}

	int getType() const { return type; }
	int getStream() const { return stream; }
	int getStatus() const { return status; }
	double getTime() const { return time; }
	double getLength() const { return length; }
	double getBW() const { return bw; }
	Kernel* getKernel() const { return kernel; }
	
	void setType(int t) { type = t; }
	void setStream(int s) { stream = s; }
	void setStatus(int s) { status = s; }
	void setTime(double t) { time = t; }
	void setLength(double l) { length = l; }
	void setBW(double f) { bw = f; }
	void setKernel(Kernel* k) {
		kernel = k;
		setType(kernel->getType());
		setStream(kernel->getStream());
		setLength(kernel->getLength());
	}
	void setEvent(Event e) {
		type = e.getType();
		stream = e.getStream();
		status = e.getStatus();
		time = e.getTime();
		length = e.getLength();
		bw = e.getBW();
		kernel = e.getKernel();
	}
};

bool operator>(const Event& e1, const Event& e2) { return e1.getTime() < e2.getTime(); }
bool operator<(const Event& e1, const Event& e2) { return e1.getTime() > e2.getTime(); }

#endif
