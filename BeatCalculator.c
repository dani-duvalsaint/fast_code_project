
#include <stdio.h>
#include <math.h>
#include <kiss_fftr.h>
#include <kiss_fft.h>

#include <immintrin.h>
#include <x86intrin.h>
#include <xmmintrin.h>


static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}


void readInWavFile(float* output, int sample_size){
	
	//Open wave file in read mode
	FILE * infile = fopen("80bpm.wav","rb");        
   // For counting number of frames in wave file.
    int count = 0;                        
    /// short int used for 16 bit as input data format is 16 bit PCM audio
    short int buff16;
   
    //printf("%s\n","start" );

    if (infile){
        fseek(infile,44,SEEK_SET);
        while (count < sample_size){
            fread(&buff16,sizeof(buff16),1,infile);        // Reading data in chunks of BUFSIZE
            output[count] = buff16;
            count++;                    
    	}
	}
}

void derivative(float* sample, float* dif_sample, int sample_size) {

    float constant = 44100.0/2;
    __m128 multiply_constant = _mm_load1_ps(&constant);
    __m128 front, back;
    int i = 1;

    unsigned long long st_1, et_1;

    st_1 = rdtsc();

    dif_sample[0] = sample[0];

    #pragma omp parallel for 
    for (i = 1; i<sample_size-6; i +=4) {
        front = _mm_loadu_ps(&sample[i+1]);
        back = _mm_loadu_ps(&sample[i-1]);
        front = _mm_sub_ps(front, back);
        front = _mm_mul_ps(front, multiply_constant);

        _mm_storeu_ps(&dif_sample[i], front);

       // i+=4;
    }
    //#pragma omp parallel for 
    for (i=i; i < sample_size - 1; i++) {
        dif_sample[i] = constant * (sample[i+1] - sample[i-1]);
        //i++;
    }

    dif_sample[sample_size - 1] = sample[sample_size-1];

    et_1 = rdtsc();

    printf ("time to take derivative: %lu\n", (et_1-st_1));


}


void fftrArray(float* sample, int size, kiss_fft_cpx* out) {
    kiss_fftr_cfg cfg;

    int i;

    if ((cfg = kiss_fftr_alloc(size, 0, NULL, NULL)) == NULL) {
        printf("Not enough memory to allocate fftr!\n");
        exit(-1);
    }

    unsigned long long st, et;
    st = rdtsc(); 
    kiss_fftr(cfg, (kiss_fft_scalar*)sample, out);
    free(cfg);

    et = rdtsc();
    printf ("time to take FFT: %lu\n", (et-st));

}

void fftArray(unsigned int* sample, int size, kiss_fft_cpx* out) {
  kiss_fft_cpx in[size/2];
  kiss_fft_cfg cfg;
  int i;

  if ((cfg = kiss_fft_alloc(size/2, 0, NULL, NULL)) == NULL) {
    printf("Not Enough Memory?!?");
    exit(-1);
  }

  //set real components to one side of stereo input, complex to other
  for(i=0; i < size; i+=2) {
    in[i/2].r = sample[i];
    in[i/2].i = sample[i+1];
  }

  kiss_fft(cfg, in, out);
  free(cfg);

}

int combfilter(kiss_fft_cpx fft_array[], int size, int sample_size, int start, int fin, int step) {
    float AmpMax =0.0001;
    int energyCount = (fin - start)/step;
    double E[energyCount];
    unsigned long long st, et;
    int count = 0;
    int i, k;
    int n = sample_size/2+1;
    /*kiss_fft_cpx *out[energyCount*(sample_size/2+1)];*/
    kiss_fft_cpx *out1= (kiss_fft_cpx*)malloc(energyCount*(n)*sizeof(kiss_fft_cpx));
    /*for (int i = 0; i < energyCount; i++){
        out[i] = (kiss_fft_cpx*)malloc((sample_size/2+1)*sizeof(kiss_fft_cpx));
    }*/

    // Iterate through all possible BPMs
    ////#pragma omp parallel for schedule(dynamic)
    for (i = 0; i < energyCount; i++) {
        int BPM = start + i * step;
        int Ti = 60 * 44100/BPM;
        float l[sample_size];
        count = 0;
        //printf("BPM: %d Sample Size: %d Ti: %d\n", BPM, sample_size, Ti);

        for (k = 0; k < sample_size; k++) {
            if ((k % Ti) == 0) {
                count++;
                l[k] = (float)AmpMax;    
            }
            else {
                l[k] = -0.0001;
            }
        }

        fftrArray(l, sample_size, &out1[i*n]);
    }

    kiss_fft_cpx *out= (kiss_fft_cpx*)malloc(energyCount*(n)*sizeof(kiss_fft_cpx));
    for (i = 0; i < (energyCount); i++) {
        for (k = 0; k < n; k++){
            out[k*energyCount+i]  = out1[i*n + k];

        }
    }
    printf("%s\n", "here");
    float a, b, a1, b1, a2, b2, a3, b3, a4, b4;
  
    int num_instr = 0;
    st = rdtsc();

    //Code for derivative

    //#pragma omp parallel for 
    for (k = 0; k < n; k++){       
        for (i = 0; i < (energyCount); i+=5) {    
    
            //if (k % 5 == 0){
            a = fft_array[k].r * out[k*energyCount + i].r - fft_array[k].i * out[k*energyCount + i].i;
            b = fft_array[k].r * out[k*energyCount + i].i + fft_array[k].i * out[k*energyCount + i].r;
            a1 = fft_array[k].r * out[k*energyCount + i+ 1].r - fft_array[k].i * out[k*energyCount + i + 1].i;
            b1 = fft_array[k].r * out[k*energyCount + i+ 1].i + fft_array[k].i * out[k*energyCount + i + 1].r;
            a2 = fft_array[k].r * out[k*energyCount + i+ 2].r - fft_array[k].i * out[k*energyCount + i + 2].i;
            b2 = fft_array[k].r * out[k*energyCount + i+ 2].i + fft_array[k].i * out[k*energyCount + i + 2].r;
            a3 = fft_array[k].r * out[k*energyCount + i+ 3].r - fft_array[k].i * out[k*energyCount + i + 3].i;
            b3 = fft_array[k].r * out[k*energyCount + i+ 3].i + fft_array[k].i * out[k*energyCount + i + 3].r;
            a4 = fft_array[k].r * out[k*energyCount + i+ 4].r - fft_array[k].i * out[k*energyCount + i + 4].i;
            b4 = fft_array[k].r * out[k*energyCount + i+ 4].i + fft_array[k].i * out[k*energyCount + i + 4].r;

            E[i] += (a*a + b*b);
            E[i+1] += (a1*a1 + b1*b1);
            E[i+2] += (a2*a2 + b2*b2);
            E[i+3] += (a3*a3 + b3*b3);
            E[i+4] += (a4*a4 + b4*b4);
        }
    }
    et = rdtsc();
    printf ("time to take energy: %lu\n", (et-st));

    printf("number of instructions: %i\n", num_instr);

    //Calculate max of E[k]
    double max_val = -1;
    //double max_val_array[];
    int index = -1; 
    int found = 0;
    //int index_array[];

    st = rdtsc();
    //#pragma omp parallel for reduction(max:max_val)
    for (i = 0; i < energyCount; i++) {
       //printf("BPM: %d \t Energy: %f\n", start + i*step, E[i]);
        if (E[i] >= max_val) {
        	if (E[i] >= max_val *1.9 && i != 0 && found == 0){
        		max_val = E[i];
            	index = i;
                found = 1;
            
        	}
            else if (found ==  1 && E[i] >= max_val *1.1){

                max_val = E[i];
                index = i;
            }
        }
    }
    et = rdtsc();
    printf ("time to find max energy: %lu\n", (et-st));
    int final = (int)((float)(start + index * step) *(float)(2.0/3.0));
    printf("final val is %i\n", final);
    return final;
}

/* detect_beat
 * Returns the BPM of the given mp3 file
 * @Params: s - the path to the desired mp3
 */
int detect_beat(int sample_size) {
    // Step 1: Get a 5-second sample of our desired mp3
    // Assume the max frequency is 4096
    unsigned long long st, et;
    st = rdtsc(); 

    int max_freq = sample_size/4.4;
    // Load mp3
    float* sample = (float*)malloc(sizeof(float) * sample_size);
    //readMP3(s, sample, sample_size);
    readInWavFile(sample, sample_size);

    //for (int i = 0; i < sample_size; i++) {
    //    printf("Element %i: %i\n", i, sample[i]);
    //}

    // Step 2: Differentiate
    float* differentiated_sample = (float*)malloc(sizeof(float) * sample_size);
    int Fs = 48000;

    /*unsigned long long st_1, et_1;
    st_1 = rdtsc(); */

    derivative(sample, differentiated_sample, sample_size);

    /*unsigned long long st_1, et_1;
    st_1 = rdtsc(); 
    difd_sample[0] = sample[0];
  
    for (int i = 1; i < sample_size - 1; i++) {
        differentiated_sample[i] = Fs * (sample[i+1]-sample[i-1])/2; 
    }
    differentiated_sample[sample_size - 1] = sample[sample_size-1];
    et_1 = rdtsc();
    printf ("time to take derivative: %lu\n", (et_1-st_1));
*/
    // Step 3: Compute the FFT
    kiss_fft_cpx out[sample_size/2+1];
    fftrArray(differentiated_sample, sample_size, out);
    printf("%s\n", "first FFT");

    //for (int i = 0; i < sample_size / 2; i++)
    //  printf("out[%2zu] = %+f , %+f\n", i, out[i].r, out[i].i);

    //printf("Combfilter performing...\n");

    int BPM = combfilter(out, sample_size / 2 + 1, sample_size, 15, 200, 1);
   //int BPM = combfilter(out, sample_size / 2 + 1, sample_size, BPM-5, BPM+5, 1);

    //printf("Final BPM: %i\n", BPM);

    // Step 4: Generate Sub-band array values
    free(sample);

    et = rdtsc();

    printf ("total time: %lu\n", (et-st));
    return BPM;
}

int main(int argc, char* argv[]) {
  // Test CPU Version
    //takes in 10 seconds at a sampling rate of 48000 samples/sec
	int sample_size = 524288;
    int BPM = detect_beat(sample_size);
	printf("Final BPM: %i\n", BPM);

	return 0;
}

