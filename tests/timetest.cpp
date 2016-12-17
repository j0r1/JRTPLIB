#include "rtptimeutilities.h"
#include <iostream>
#include <stdio.h>

using namespace std;
using namespace jrtplib;

int main(void)
{
	double times[] = { 1.2, -2.3, 0, 3.999999 };

	for (int i = 0 ; i < sizeof(times)/sizeof(double) ; i++)
	{
		RTPTime t(times[i]);

		cout << "Double:       " << times[i] << endl;
		cout << "Seconds:      " << t.GetSeconds() << endl;
		cout << "Microseconds: " << t.GetMicroSeconds() << endl;
		cout << endl;
	}

	int32_t secsAndMuSecs[][2] = { { 1, 3000}, {-2, 200}, {0, 0}, {-2, 999999} };
	for (int i = 0 ; i < sizeof(secsAndMuSecs)/(2*sizeof(int32_t)) ; i++)
	{
		RTPTime t(secsAndMuSecs[i][0], secsAndMuSecs[i][1]);

		cout << "Input Seconds:       " << secsAndMuSecs[i][0] << endl;
		cout << "Input Microseconds:  " << secsAndMuSecs[i][1] << endl;
		cout << "Output Seconds:      " << t.GetSeconds() << endl;
		cout << "Output Microseconds: " << t.GetMicroSeconds() << endl;
		cout << "Output Double:       " << t.GetDouble() << endl;
		cout << endl;
	}

	for (int i = 0 ; i < 10 ; i++)
	{
		RTPTime now = RTPTime::CurrentTime();
		fprintf(stderr, "%10u.%06d\n", (uint32_t)now.GetSeconds(), now.GetMicroSeconds());
		RTPTime::Wait(RTPTime(1));
	}
	return 0;
}
