/*  Written by Tucker Vento

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
    eighth parameter: loop write count: like grain #, for loops */

//  TODO in new algo: reverse, grain density?

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void reverseBuffer(int16_t* p_buf, uint32_t p_size) {
    unsigned int i = 0;
    unsigned int j = p_size-2;
    printf("i: %u; j: %u; p_size: %u; p_buf[i]: %d; p_buf[j]: %d\n", i, j, p_size, p_buf[i], p_buf[j]);
    while (i < j) { //sample frame is 2 samples (stereo, L/R) so we should move two at once?
        int16_t d = p_buf[i];
        p_buf[i++] = p_buf[j]; //i is now at second index
        //if (p_buf[j] != 0) { printf("swapping %d with %d\n", d, p_buf[j]); }
        p_buf[j++] = d; //j is now at second index

        d = p_buf[i];
        p_buf[i++] = p_buf[j]; //i is now at first index of next sample
        //if (p_buf[j] != 0) { printf("second swapping %d with %d\n", d, p_buf[j]); }
        p_buf[j--] = d; //j is now at first index
        j -= 2; //j is now at first index of next sample
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

    char *filename;
    char *outfilename;
    char *outext;
    FILE *fp;
    FILE *fpout;

    const uint8_t REPEATMAX = (uint8_t)atoi(argv[2]);
    const uint32_t GRAINTIME = (uint32_t)atoi(argv[3]);
    const uint8_t ATTACKTIME = (uint8_t)atoi(argv[4]);
    const uint8_t REVERSEGRAINS = (uint8_t)atoi(argv[5]);
    const uint8_t SEEKTHRU = (uint8_t)atoi(argv[6]);
    const uint8_t LOOPSIZE = (uint8_t)atoi(argv[7]);
    const uint8_t LOOPMAX = (uint8_t)atoi(argv[8]);

    uint8_t _bitsPerSample;
    uint8_t _channels;
    uint32_t _sampleRate;
    uint32_t _sizeToRead;
    uint32_t _newFileDataSize;
    uint32_t _grainSize;
    uint32_t _attackSize;
    uint8_t _sampleSize;
    uint32_t _actualGrainTime;

    //grain loop variables
    uint32_t sizeToRead;                //counter for the grain read loop
    int16_t leftSample, rightSample;
    uint8_t repeatCount = 0;            //counter for repeating grains
    uint32_t attackCounter;             //counter for attack calculation
    uint8_t loopCount = 0;              //counter for macro loop
    uint8_t grainLoopCount = 0;         //counter for grains within macro loop

    if (argc != 9)
    {
        printf("./granulator filename.wav [grain repeat #] [grain time (in whole ms)] [attack time (0-100 as percent of grain time)] ");
        printf("[reverse grains (1 or 0)] [seek thru (1 or 0)] [loop size (in grains) (1 for no loop)] [loop write count (1 to not loop)]\n");
        return 1;
    }

    //formatting output filename to include parameter values
    outfilename = malloc(100);
    outext = malloc(100);
    sprintf(outext, "_%sr%sms%sa%srev%ss%sls%slc.wav", argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
    strncat(outfilename, argv[1], (strlen(argv[1]) - 4));
    strcat(outfilename, outext);

    filename = argv[1];
    printf("INPUT: %s\nOUTPUT: %s\n", filename, outfilename);

    fp = fopen(filename, "rb");
    fpout = fopen(outfilename, "wb");

    free(outext);

    if (fp == NULL) {
        printf("Problem opening input file, aborting\n");
        fclose(fpout);
        remove(outfilename);
        free(outfilename);
        return 98;
    }
    else if (fpout == NULL) {
        printf("Problem opening output file, aborting\n");
        fclose(fp);
        remove(outfilename);
        free(outfilename);
        return 99;
    }

    //read first RIFF chunk, check id and data
    if (fread(&buf, sizeof(char), 12, fp) != 12 || strncmp(buf.riff.id, "RIFF", 4) != 0 || strncmp(buf.riff.data, "WAVE", 4) != 0) {
        printf("ERROR: in reading RIFF chunk\n");
        fclose(fp);
        fclose(fpout);
        remove(outfilename);
        free(outfilename);
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
        printf("ERROR: in reading header of fmt chunk\n");
        fclose(fp);
        fclose(fpout);
        remove(outfilename);
        free(outfilename);
        return 3;
    }
    //copy fmt header to new file
    fwrite(&buf, sizeof(char), 8, fpout);
    //get fmt
    if (fread(&buf, sizeof(char), buf.riff.size, fp) != 16) {
        printf("ERROR: in reading fmt chunk\n");
        fclose(fp);
        fclose(fpout);
        remove(outfilename);
        free(outfilename);
        return 4;
    }
    //save fmt
    fwrite(&buf, sizeof(char), 16, fpout);

    //hold onto important values
    _bitsPerSample = buf.fmt.bitsPerSample;

    if (_bitsPerSample != 16) {
        printf("ERROR: %u bits per sample | ONLY 16-BIT IS ALLOWED\n", _bitsPerSample);
        fclose(fp);
        fclose(fpout);
        remove(outfilename);
        free(outfilename);
        return 7;
    }

    _channels = buf.fmt.channels;
    _sampleRate = buf.fmt.sampleRate;
    _grainSize = (buf.fmt.bytesPerSecond*GRAINTIME/1000); //in bytes; we get _grainSize by multiplying time by bytes/ms
    _sampleSize = _channels * _bitsPerSample / 8; //in bytes, should be 4

    if (_sampleSize != 4) {
        printf("ERROR: %u channels and %u bits per sample; needs to be 2 and 16\n", _channels, _bitsPerSample);
        fclose(fp);
        fclose(fpout);
        remove(outfilename);
        free(outfilename);
        return 216;
    }

    _grainSize = _grainSize - (_grainSize % _sampleSize);
    _attackSize = _grainSize * (ATTACKTIME/100.0) / 4; //divide by four (2*2) to do the attack by stereo sample instead of by byte
    _actualGrainTime = _grainSize/(buf.fmt.bytesPerSecond/1000); //is this even useful?  in case the original grain size is too "fine"

    //read header of data
    if (fread(&header, sizeof(char), 8, fp) != 8 || strncmp(header.id, "data", 4) != 0) {
        printf("ERROR: in data header reading\n");
        fclose(fp);
        fclose(fpout);
        remove(outfilename);
        free(outfilename);
        return 5;
    }
    header.size = _newFileDataSize;
    //copy header to output
    fwrite(&header, 1, 8, fpout);
    printf("_sizeToRead = %u\n_newFileDataSize = %u\n", _sizeToRead, _newFileDataSize);
    //variables for granulation loop
    sizeToRead = _grainSize;
    attackCounter = 0;
    long fpChecker = ftell(fp); //this keeps track of the read point for the current grain
    long loopPoint = fpChecker;

    while (feof(fp) == 0) {
        while (sizeToRead > 0) {    //grain read loop
            fread(&leftSample, 2, 1, fp);
            fread(&rightSample, 2, 1, fp);
            if (attackCounter < _attackSize) {
                leftSample = (double)leftSample * ((double)attackCounter/(double)_attackSize);
                rightSample = (double)rightSample * ((double)attackCounter/(double)_attackSize);
                attackCounter++;
            }
            fwrite(&leftSample, 2, 1, fpout);
            fwrite(&rightSample, 2, 1, fpout);
            sizeToRead -= 4;
        }
        attackCounter = 0;
        repeatCount++;
        sizeToRead = _grainSize;
        //seek back if we need to repeat
        if (repeatCount < REPEATMAX) { fseek(fp, fpChecker, SEEK_SET); }
        else {  //otherwise, check the macro loop
            repeatCount = 0;
            grainLoopCount++;
            if (grainLoopCount == LOOPSIZE) {
                grainLoopCount = 0;
                loopCount++;
                //check if we need to loop back to the start of the lop point
                if (loopCount < LOOPMAX) { fseek(fp, loopPoint, SEEK_SET); }
                else {
                    loopCount = 0;
                    //only seeking at the end of the macro loop for now
                    //maybe we could do this all over to make it more interesting?
                    if (SEEKTHRU != 0 && feof(fp) == 0) { fseek(fp, ftell(fpout), SEEK_SET); }
                    loopPoint = ftell(fp);
                }
            }
            fpChecker = ftell(fp);
        }
    }

    printf("\n_grainSize = %u\n", _grainSize);
    printf("_actualGrainTime = %u\n", _actualGrainTime);
    printf("_attackSize = %u\n", _attackSize);
    printf("LOOPMAX = %u\nLOOPSIZE = %u\n", LOOPMAX, LOOPSIZE);
    printf("_newFileDataSize = %u\n", _newFileDataSize);

    fclose(fp);
    fclose(fpout);
    free(outfilename);

    return 0;
}