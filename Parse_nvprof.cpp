#include<iostream>
#include<sstream>
#include<string>
#include<vector>

using namespace std;

int main() {
	string line;
	string token;
	string temp;
	double value;
	int i;

	while(cin.eof() == false) {
		getline(cin, line);
		if(line[0] == '=' || line[0] == '"' || line[0] == 's') continue;
		stringstream ss;
		ss << line;

		for(i=0; i<20; i++) {
			getline(ss, temp, ',');
			if(i == 1) cout << temp << " ";
			else if(i == 11) {
				stringstream ss2;
				ss2 << temp;
				ss2 >> value;
				if(temp.length() == 0) value = 0;
				if(i == 11) value *= 1048576;
				cout << value << " ";
			} else if(i == 18) {
				cout << temp;
				while(ss.eof() == false) {
					getline(ss, temp, ',');
					if(ss.eof()) break;
					cout << "," << temp;
				}
				cout << endl;
			}
		}
	}
}
