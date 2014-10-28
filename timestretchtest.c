
#include <stdio.h>
#include <math.h>

int main(int argc, char *argv[]) {
    int intervalSum = 0, intervalValue = 0, intervalCount = 0, INTERVAL, floorValue, ceilValue, floorCount = 0, ceilCount = 0, i;
    double intervalAverage, n;

    INTERVAL = atoi(argv[1]);

    n = 1.0 / (100.0/(double)INTERVAL - 1.0);
    intervalValue = ceil(n);
    ceilValue = intervalValue;
    floorValue = floor(n);

    for (i = 0; i < 100-INTERVAL; i++) {
        intervalSum += intervalValue;
        intervalCount++;
        intervalAverage = (double)intervalSum / (double)intervalCount;

        if (intervalValue == ceilValue) { 
            ceilCount++;
            if (intervalAverage > n) { intervalValue--; }
        }
        else if (intervalValue == floorValue) { 
            floorCount++;
            if (intervalAverage < n) { intervalValue++; }
        }
    }

    printf("INTERVAL = %d\nn = %lf\n\nceilCount = %d\nfloorCount = %d\n", INTERVAL, n, ceilCount, floorCount);
}