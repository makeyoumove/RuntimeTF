#include<iostream>
#include<sstream>
#include<string>
#include<vector>

using namespace std;

int main() {
	vector<string> name;
	vector<vector<double> > duration;
	vector<vector<double> > sizes;
	vector<double> vtemp;
	string line, temp;
	double v1, v2;
	int nk = 0, i, j;

	while(cin.eof() == false) {
		stringstream ss;
		getline(cin, line);

		ss << line;
		ss >> v1 >> v2;
		getline(ss, temp);

//		cout << temp << " " << v1 << " " << v2 << endl;
		if(nk == 0) {
			name.push_back(temp);
			vector<double> nv1;
			nv1.push_back(v1);
			duration.push_back(nv1);
			vector<double> nv2;
			nv2.push_back(v2);
			sizes.push_back(nv2);
			nk++;
		}
		else {
			for(i=0; i<nk; i++) 
				if(name[i] == temp) break;

			if(i == nk) {
				name.push_back(temp);
				vector<double> nv1;
				nv1.push_back(v1);
				duration.push_back(nv1);
				vector<double> nv2;
				nv2.push_back(v2);
				sizes.push_back(nv2);
				nk++;
			}
			else {
				duration[i].push_back(v1);
				sizes[i].push_back(v2);
			}
		}
	}

	for(i=0; i<nk; i++) {
		for(j=0; j<i; j++) {
			if(duration[i].size() > duration[j].size()) {
				temp = name[i]; name[i] = name[j] ; name[j] = temp;
				vtemp = duration[i]; duration[i] = duration[j]; duration[j] = vtemp;
				vtemp = sizes[i]; sizes[i] = sizes[j]; sizes[j] = vtemp;
			}
		}
	}

	cout << nk << endl;
	for(i=0; i<nk; i++) {
		cout << duration[i].size() << " ";
		double avg1 = 0, avg2 = 0;
		for(j=0; j<duration[i].size(); j++) avg1 += duration[i][j];
		avg1 /= duration[i].size();
		for(j=0; j<sizes[i].size(); j++) avg2 += sizes[i][j];
		avg2 /= sizes[i].size();
		cout << avg1 << " " << avg2 << " " << name[i] << endl;
	}
}
