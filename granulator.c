/*
    Written by Tucker Vento

    Command line granulation processing
    Outputs a file called processed.wav

    first parameter: filename (including .wav extension)
    second parameter: grain write #: how many times to write each grain
    third parameter: grain size: in whole milliseconds
    fourth parameter: attack time: 0-100, as a percent of the grain time
    fifth parameter: reverse grains: 1 or 0, reverses each grain in place, but plays forward (not working)
    sixth parameter: seek thru: 1 or 0, when used in conjunction with grain-repeating it will seek the length of data written per grain,
        and pull the next grain from that distance away in the original track.  the new track will be the same length as the original
    seventh parameter: loop size: 1+, 1 gives no loop
    eighth parameter: loop write count: like grain #, for loops 
*/

//TODO############ FIX REVERSING
//LOOP SEEKING working, verified 9/2

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void reverseBuffer(int16_t p_buf[], uint32_t p_size) {
    int i = 0;
    int j = p_size-1;//is this the source of our problem?  p_size is uint32_t and could overflow when converted to int...
    while (i < j) {
        int16_t d = p_buf[i];
        p_buf[i++] = p_buf[j];
        p_buf[j--] = d;
    }
}

int main(int argc, char *argv[]) {
    union {
      struct {
        char     id[4];
        uint32_t size;
        char     data[4];
      } riff;  // riff chunk
      struct {
        uint16_t compress;
        uint16_t channels;
        uint32_t sampleRate;
        uint32_t bytesPerSecond;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
        uint16_t extraBytes;
      } fmt; // fmt data
    } buf;

    struct {
        char id[4];
        uint32_t size;
    } header;

    //const int BUFFERLEN = 512;

    char *filename;
    char *outfilename;
    char *outext;
    FILE *fp;
    FILE *fpout;

    uint8_t _bitsPerSample;
    uint8_t _channels;
    uint32_t _sampleRate;

    uint32_t _sizeToRead;
    uint32_t _newFileDataSize;
    const uint8_t REPEATMAX = (uint8_t)atoi(argv[2]);//2;
    const uint32_t GRAINTIME = (uint32_t)atoi(argv[3]);//8;
    const uint8_t ATTACKTIME = (uint8_t)atoi(argv[4]);
    const uint8_t REVERSEGRAINS = (uint8_t)atoi(argv[5]);
    const uint8_t SEEKTHRU = (uint8_t)atoi(argv[6]);
    const uint8_t LOOPSIZE = (uint8_t)atoi(argv[7]);
    const uint8_t LOOPMAX = (uint8_t)atoi(argv[8]);
    uint32_t _grainSize;
    uint32_t _attackSize;

    //grain loop variables
    uint32_t readRemaining;
    uint32_t writeRemaining;
    uint32_t sizeToRead;
    int16_t *grainBuffer;
    uint32_t totalGrainCount = 0;
    uint8_t repeatCount = 0;
    uint32_t attackCounter;
    uint8_t loopCount = 0;
    uint8_t grainLoopCount = 0;

    if (argc != 9)
    {
        printf("./granulator filename.wav [grain repeat #] [grain time (in whole ms)] [attack time (0-100 as percent of grain time)] ");
        printf("[reverse grains (1 or 0)] [seek thru (1 or 0)] [loop size (in grains) (1 for no loop)] [loop write count (1 to not loop)]\n");
        return 1;
    }

    //formatting output filename
    outfilename = malloc(100);
    outext = malloc(100);
    sprintf(outext, "_%sr%sms%sa%srev%ss%sls%slc.wav", argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
    strncat(outfilename, argv[1], (strlen(argv[1]) - 4));
    strcat(outfilename, outext);

    filename = argv[1];
    printf("INPUT: %s\n", filename);
    printf("OUTPUT: %s\n", outfilename);

    fp = fopen(filename, "r");
    fpout = fopen(outfilename, "w");

    free(outfilename);
    free(outext);

    if (fp == NULL || fpout == NULL) {
        printf("Problem opening files, aborting\n");
        return 99;
    }

    //read first RIFF chunk, check id and data
    if (fread(&buf, sizeof(char), 12, fp) != 12 || strncmp(buf.riff.id, "RIFF", 4) != 0 || strncmp(buf.riff.data, "WAVE", 4) != 0) {
        return 2;
    }

    _sizeToRead = buf.riff.size - 36;
    _newFileDataSize = (_sizeToRead * SEEKTHRU) + (_sizeToRead * REPEATMAX * (1 - SEEKTHRU) * LOOPMAX);
    buf.riff.size = _newFileDataSize + 36;

    //copy header to new file
    fwrite(&buf, sizeof(char), 12, fpout);
    printf("size: %u\n", buf.riff.size);
    //read header of fmt chunk, verify it's the fmt chunk
    if (fread(&buf, sizeof(char), 8, fp) != 8 || strncmp(buf.riff.id, "fmt ", 4) != 0 || (buf.riff.size != 16 && buf.riff.size != 18)) {
        return 3;
    }
    //copy fmt header to new file
    fwrite(&buf, sizeof(char), 8, fpout);
    //get fmt
    if (fread(&buf, sizeof(char), buf.riff.size, fp) != 16) {
        return 4;
    }
    //save fmt
    fwrite(&buf, sizeof(char), 16, fpout);

    //hold onto important values
    _bitsPerSample = buf.fmt.bitsPerSample;
    _channels = buf.fmt.channels;
    _sampleRate = buf.fmt.sampleRate;
    _grainSize = (buf.fmt.bytesPerSecond/1000*GRAINTIME);
    _attackSize = _grainSize * (ATTACKTIME/100.0);

    //read header of data
    if (fread(&header, sizeof(char), 8, fp) != 8 || strncmp(header.id, "data", 4) != 0) {
        return 5;
    }
    header.size = _newFileDataSize;
    //copy header to outputi
    fwrite(&header, 1, 8, fpout);
    printf("_sizeToRead = %u\n_newFileDataSize = %u\n", _sizeToRead, _newFileDataSize);
    //variables for granulation loop
    readRemaining = _sizeToRead;
    writeRemaining = _newFileDataSize;
    sizeToRead = _grainSize;
    grainBuffer = (int16_t*)malloc(sizeof(int16_t)*_grainSize);
    long fpChecker = ftell(fp);
    long loopPoint;

    while (readRemaining > 0 && writeRemaining > 0) { 
        loopCount = 0;
        loopPoint = ftell(fp);
        while (loopCount < LOOPMAX && writeRemaining > 0) {
            grainLoopCount = 0;
            while (grainLoopCount < LOOPSIZE && writeRemaining > 0) {
                fpChecker = ftell(fp);
                size_t readAmt = fread(grainBuffer, 1, sizeToRead, fp);
                fpChecker = ftell(fp);
                if (readAmt != sizeToRead) { free(grainBuffer); printf("READ ERROR\n%zu read\n", readAmt); return 6; }

                if (ATTACKTIME > 0) {
                    for (attackCounter = 0; attackCounter < sizeToRead && attackCounter < _attackSize; attackCounter++) {
                        //very coarse attack...just a linear slope from 0 to 100 at _attackSize
                        grainBuffer[attackCounter] = (double)grainBuffer[attackCounter]*((double)attackCounter/(double)_attackSize);
                    }
                }

                if (REVERSEGRAINS == 1) { reverseBuffer(grainBuffer, sizeToRead); } //not working
                //write out grains REPEATMAX times
                for (repeatCount = 0; repeatCount < REPEATMAX && readRemaining >= sizeToRead && writeRemaining > 0; repeatCount++) {
                    fwrite(grainBuffer, 1, sizeToRead, fpout);
                    writeRemaining -= sizeToRead;
                    totalGrainCount++;
                    readRemaining -= sizeToRead * SEEKTHRU;
                    if (sizeToRead > readRemaining) { sizeToRead = readRemaining; } 
                }
                grainLoopCount++;
                //change sizeToRead if EOF coming up
                readRemaining -= sizeToRead * (1 - SEEKTHRU);
                if (sizeToRead > readRemaining) { sizeToRead = readRemaining; printf("changing sizeToRead: %d\n", sizeToRead); }
            }
            loopCount++;
            fpChecker = ftell(fp);
            //instead of having a state-based loop/seek could we do this every time and handle it afterwards..
            if (LOOPMAX > 1 && loopCount != LOOPMAX)
                { fseek(fp, loopPoint, SEEK_SET); readRemaining += sizeToRead*grainLoopCount*(1 - SEEKTHRU); }
            fpChecker = ftell(fp);
        } 
        if (SEEKTHRU != 0 && readRemaining > sizeToRead && writeRemaining > 0) { fseek(fp, ftell(fpout), SEEK_SET); } //seekthru if set
        printf("readRemaining = %u\nsizeToRead = %u\ntotalGrainCount = %u\nwriteRemaining = %u\n", readRemaining, sizeToRead, totalGrainCount, writeRemaining);
    }
    printf("\ntotalGrainCount = %u\n", totalGrainCount);
    printf("_grainSize = %u\n", _grainSize);
    printf("_attackSize = %u\n", _attackSize);
    printf("LOOPMAX = %u\nLOOPSIZE = %u\n", LOOPMAX, LOOPSIZE);
    printf("_newFileDataSize = %u\n", _newFileDataSize);
    /*when loopsize grains have been written, seek
    backwards loopsize * grainsize and repeat loopcount times
    include looping in the seek forward calculation, which should take place after the loop structure*/

    fclose(fp);
    fclose(fpout);
    free(grainBuffer);

    return 0;
}