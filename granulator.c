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
*/

//TODO############LOOPING (macro loop over the grains) and FIX REVERSING

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void reverseBuffer(int16_t p_buf[], uint32_t p_size) {
    int i = 0;
    int j = p_size-1;
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
    char *outname = "processed.wav";
    FILE *fp;
    FILE *fpout;

    uint8_t _bitsPerSample;
    uint8_t _channels;
    uint32_t _sampleRate;

    if (argc != 7)
    {
        printf("./granulator filename.wav [grain repeat #] [grain time (in whole ms)] [attack time (0-100 as percent of grain time)] [reverse grains (1 or 0)] [seek thru (1 or 0)]\n");
        return 1;
    }

    uint32_t _sizeToRead;
    uint32_t _newFileDataSize;
    const uint8_t REPEATMAX = atoi(argv[2]);//2;
    const uint32_t GRAINTIME = atoi(argv[3]);//8;
    const uint8_t ATTACKTIME = atoi(argv[4]);
    const uint8_t REVERSEGRAINS = atoi(argv[5]);
    const uint8_t SEEKTHRU = atoi(argv[6]);
    uint32_t _grainSize;
    uint32_t _attackSize;

    filename = argv[1];
    printf("%s\n", filename);

    fp = fopen(filename, "r");
    fpout = fopen(outname, "w");

    //read first RIFF chunk, check id and data
    if (fread(&buf, sizeof(char), 12, fp) != 12 || strncmp(buf.riff.id, "RIFF", 4) || strncmp(buf.riff.data, "WAVE", 4)) {
        return 2;
    }

    _sizeToRead = buf.riff.size - 36;
    _newFileDataSize = _sizeToRead + _sizeToRead * (REPEATMAX - 1) * (1 - SEEKTHRU);
    buf.riff.size = _newFileDataSize + 36;

    //copy header to new file
    fwrite(&buf, sizeof(char), 12, fpout);
    printf("size: %d\n", buf.riff.size);
    //read header of fmt chunk, verify it's the fmt chunk
    if (fread(&buf, sizeof(char), 8, fp) != 8 || strncmp(buf.riff.id, "fmt ", 4) || (buf.riff.size != 16 && buf.riff.size != 18)) {
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
    if (fread(&header, sizeof(char), 8, fp) != 8 || strncmp(header.id, "data", 4)) {
        return 5;
    }
    header.size = _newFileDataSize;
    //copy header to output
    fwrite(&header, 1, 8, fpout);
    //save size of data, get bufferlength (512)
    printf("grain remainder: %d\n", header.size%512);
    printf("_sizeToRead = %d\n_newFileDataSize = %d\n", _sizeToRead, _newFileDataSize);
    //variables for granulation loop
    uint32_t readRemaining = _sizeToRead;
    uint32_t sizeToRead = _grainSize;
    int16_t grainBuffer[_grainSize];
    uint32_t totalGrainCount = 0;
    uint8_t repeatCount = 0;
    uint32_t attackCounter;
    uint8_t stillReading = 1;

    while (readRemaining > 0 && stillReading != 0) { 
        if (fread(grainBuffer, 1, sizeToRead, fp) != sizeToRead) { return 6; }
        if (ATTACKTIME > 0) {
            for (attackCounter = 0; attackCounter < sizeToRead && attackCounter < _attackSize; attackCounter++) {
                //very coarse attack...just a linear slope from 0 to 100 at _attackSize
                grainBuffer[attackCounter] = (double)grainBuffer[attackCounter]*((double)attackCounter/(double)_attackSize);
            }
        }
        if (REVERSEGRAINS) { reverseBuffer(grainBuffer, sizeToRead); } //not working
        //write out grains REPEATMAX times
        //if (readRemaining < sizeToRead + sizeToRead * (REPEATMAX - 1) * (SEEKTHRU)) { printf("exiting grain loop early\n"); break; }
        for (repeatCount = 0; repeatCount < REPEATMAX && readRemaining > sizeToRead; repeatCount++) {
            fwrite(grainBuffer, 1, sizeToRead, fpout);
            totalGrainCount++;
            readRemaining -= sizeToRead * SEEKTHRU;
        }
        
        readRemaining -= sizeToRead * (1 - SEEKTHRU);
        if (sizeToRead > readRemaining) { sizeToRead = readRemaining; printf("%d\n", sizeToRead); } //change sizeToRead if EOF coming up
        if (SEEKTHRU) { fseek(fp, sizeToRead, SEEK_CUR); } //seekthru if set
        printf("readRemaining = %d\nsizeToRead = %d\ntotalGrainCount = %d\n", readRemaining, sizeToRead, totalGrainCount);
    }
    printf("\ntotalGrainCount = %d\n", totalGrainCount);
    printf("_grainSize = %d\n", _grainSize);
    printf("_attackSize = %d\n", _attackSize);

    fclose(fp);
    fclose(fpout);

    return 0;
}