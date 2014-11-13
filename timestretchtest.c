
#include <stdio.h>
#include <math.h>

int main(int argc, char *argv[]) {
    int intervalSum = 0, intervalValue = 0, intervalCount = 0, INTERVAL, floorValue, ceilValue;
    int floorCount = 0, ceilCount = 0, i, writes, duplicateBase, duplicateCount, writeCount = 0, intervalCounter = 0;
    double intervalAverage, n, duplicatePer;

    INTERVAL = atoi(argv[1]);
    duplicatePer = (100.0/(double)INTERVAL - 1.0);
    if (duplicatePer >= 1.0) { duplicateBase = floor(duplicatePer); duplicatePer = duplicatePer - (double)duplicateBase; }
    n = 1.0 / duplicatePer;
    intervalValue = ceil(n);
    ceilValue = intervalValue;
    floorValue = floor(n);

    for (i = 0; i < 100; i++) {
        //do normal write here
        writeCount++;

        duplicateCount = duplicateBase;

        if (intervalCounter % intervalValue == 0) {
            printf("intervalValue = %d\n", intervalValue);
            duplicateCount++;

            intervalSum += intervalValue;
            intervalCount++;
            intervalCounter = 0;

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
        while (duplicateCount > 0) {
            //calculate sample based on duplicate # / total dupes and scale sample difference based on that
            writeCount++;
            duplicateCount--;
        }
        intervalCounter++;
    }

    printf("INTERVAL = %d\nduplicateBase = %d\nn = %lf\n\nwriteCount = %d\nceilCount = %d\nfloorCount = %d\n", INTERVAL, duplicateBase, n, writeCount, ceilCount, floorCount);
}