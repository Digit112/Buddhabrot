#include <algorithm>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "ISAAC/rand.c"

// Used for storing an array of float points and associated max counts.
// This is very important because we will need to sort an array of three of these.
// This would require a shit ton of logic without a class.
class cnt_max_iter {
public:
	int* cnt;
	int max_iters;
	
	cnt_max_iter(int width, int height, int max_iters) : cnt(NULL), max_iters(max_iters) {
		cnt = new int[width*height];
		
		if (cnt == NULL) {
			printf("Error allocating counter.\n");
			exit(1);
		}
		
		for (int i = 0; i < width*height; i++) {
			cnt[i] = 0;
		}
	}
	
	~cnt_max_iter() {
		if (cnt != NULL) {
			delete[] cnt;
		}
	}
};

// qsort isn't working for some reason so I use this instead.
// It's basically hard-coded bubble sort for a 3-element array.
void sort_cnts(cnt_max_iter** cnts) {
	cnt_max_iter* temp;
	
	if (cnts[0]->max_iters > cnts[1]->max_iters) {
		temp = cnts[0];
		cnts[0] = cnts[1];
		cnts[1] = temp;
	}
	
	if (cnts[1]->max_iters > cnts[2]->max_iters) {
		temp = cnts[1];
		cnts[1] = cnts[2];
		cnts[2] = temp;
	}
	
	if (cnts[0]->max_iters > cnts[1]->max_iters) {
		temp = cnts[0];
		cnts[0] = cnts[1];
		cnts[1] = temp;
	}
}

int main() {
	int width = 1026;
	int height = 1368;
	
	int max_points_per_pass = 1000000;
	int num_passes = 10000;
	
	// Camera position.
	float cx = 0;
	float cy = -0.6;
	
	float cw = 2.4;
	float ch = 3.2;
	
	// Max iters per RGB channel.
	int max_iters_r = 50;
	int max_iters_g = 500;
	int max_iters_b = 5000;
	
	// The number of entries in the histogram will be ceil(max_cnt / histogram_scale).
	int histogram_scale = 1;
	
	// Allocate space for the counters.
	printf("Allocating Counters...\n");
	
	cnt_max_iter cnt_r(width, height, max_iters_r);
	cnt_max_iter cnt_g(width, height, max_iters_g);
	cnt_max_iter cnt_b(width, height, max_iters_b);
	
	printf("Sorting counters...\n");
	
	printf("Iters: %d, %d, %d\n", cnt_r.max_iters, cnt_g.max_iters, cnt_b.max_iters);
	
	// Get a list of pointers to the counters and sort it so we know which order they appear in.
	cnt_max_iter* cnts[3] = {&cnt_r, &cnt_g, &cnt_b};
	sort_cnts(cnts);
	
	int max_iters = cnts[2]->max_iters;
	
	printf("Iters: %d, %d, %d -> ", cnt_r.max_iters, cnt_g.max_iters, cnt_b.max_iters);
	printf("%d, %d, %d\n", cnts[0]->max_iters, cnts[1]->max_iters, cnts[2]->max_iters);
	printf("%p, %p, %p -> %p, %p, %p\n", cnt_r.cnt, cnt_g.cnt, cnt_b.cnt, cnts[0]->cnt, cnts[1]->cnt, cnts[2]->cnt);
	
	// Allocate space for one point's trail.
	float* real = new float[max_iters];
	float* imag = new float[max_iters];
	
	if (real == NULL || imag == NULL) {
		printf("Error allocating point buffer.\n");
		exit(1);
	}
	
	randctx ctx;
	randinit(&ctx, false);
	
	// Iterate the points.
	srand(0);
	float Cr;
	float Ci;
	for (int m = 0; m < num_passes; m++) {
		printf("Pass %d of %d...\n", m, num_passes);
		
		for (int p = 0; p < max_points_per_pass; p++) {
			// Randomly regenerate points until one isn't in the
			// Main cardioid or the second-period bulb
			for (int i = 0; i < 10; i++) {
				Cr = (float) (ISAAC_rand(&ctx) % 4194304) / 4194304 * 6 - 3;
				Ci = (float) (ISAAC_rand(&ctx) % 4194304) / 4194304 * 6 - 3;
				
				// Skip this point if it is within the main cardioid or period two bulb.
				// If the thresholds specified here are precisely what they should be (0 and 0.0625 instead of -0.01 and 0.062),
				// then the Buddhabrot will have cake. Don't believe me? Try it.
				float crm = Cr - 0.25;
				float crp = Cr + 1;
				float crm2_Ci2 = crm*crm + Ci*Ci;
				if (crm2_Ci2*crm2_Ci2 + 0.25 * crm * crm2_Ci2 - 0.25 * Ci*Ci > -0.01 && crp*crp + Ci*Ci > 0.062) {
					break;
				}
			}
			
			real[0] = Cr;
			imag[0] = Ci;
			
			float r_i_2;
			float i_i_2;
			for (int i = 1; i < max_iters; i++) {
				// Calculates the square of the last elements to be generated.
				r_i_2 = real[i-1]*real[i-1];
				i_i_2 = imag[i-1]*imag[i-1];
				
				// If the point escapes, write the locations it visited to the counter grid.
				if (r_i_2 + i_i_2 > 240) {
					// Write the trail only to the appropriate channels.
					if (i < cnts[0]->max_iters) {
						for (int j = 0; j < i; j++) {
							int x = floor((imag[j] - (cx - cw / 2)) / cw * width);
							int y = floor((real[j] - (cy - ch / 2)) / ch * height);
							
							if (x >= 0 && x < width && y >= 0 && y < height) {
								cnts[0]->cnt[y*width + x]++;
								cnts[1]->cnt[y*width + x]++;
								cnts[2]->cnt[y*width + x]++;
							}
						}
					}
					else if (i < cnts[1]->max_iters) {
						// Write the trail only to the appropriate channels.
						for (int j = 0; j < i; j++) {
							int x = floor((imag[j] - (cx - cw / 2)) / cw * width);
							int y = floor((real[j] - (cy - ch / 2)) / ch * height);
							
							if (x >= 0 && x < width && y >= 0 && y < height) {
								cnts[1]->cnt[y*width + x]++;
								cnts[2]->cnt[y*width + x]++;
							}
						}
					}
					else {
						// Write the trail only to the appropriate channels.
						for (int j = 0; j < i; j++) {
							int x = floor((imag[j] - (cx - cw / 2)) / cw * width);
							int y = floor((real[j] - (cy - ch / 2)) / ch * height);
							
							if (x >= 0 && x < width && y >= 0 && y < height) {
								cnts[2]->cnt[y*width + x]++;
							}
						}
					}
					
					// Stop iterating this point.
					break;
				}
				
				// Calculate the next location.
				real[i] = r_i_2 - i_i_2 + Cr;
				imag[i] = 2*real[i-1]*imag[i-1] + Ci;
			}
		}
	}
	
	printf("a\n");
	
	// Get the highest value in the counters.
	int max_cnt_r = 0;
	int max_cnt_g = 0;
	int max_cnt_b = 0;
	for (int i = 0; i < width*height; i++) {
		if (max_cnt_r < cnt_r.cnt[i]) {
			max_cnt_r = cnt_r.cnt[i];
		}
		
		if (max_cnt_g < cnt_g.cnt[i]) {
			max_cnt_g = cnt_g.cnt[i];
		}
		
		if (max_cnt_b < cnt_b.cnt[i]) {
			max_cnt_b = cnt_b.cnt[i];
		}
	}
	
	printf("Max Counts: %d, %d, %d\n", max_cnt_r, max_cnt_g, max_cnt_b);
	
	// Allocate the histograms for each channel.
	printf("Creating Histograms...\n");
	
	int hist_len_r = max_cnt_r / histogram_scale + 1;
	int hist_len_g = max_cnt_g / histogram_scale + 1;
	int hist_len_b = max_cnt_b / histogram_scale + 1;
	
	int* hist_r = new int[hist_len_r];
	int* hist_g = new int[hist_len_g];
	int* hist_b = new int[hist_len_b];
	
	if (hist_r == NULL || hist_g == NULL || hist_b == NULL) {
		printf("Error allocating histograms.\n");
		exit(1);
	}
	
	// Fill each histogram with zeros.
	printf("Clearing Histograms...\n");
	for (int i = 0; i < hist_len_r; i++) hist_r[i] = 0;
	for (int i = 0; i < hist_len_g; i++) hist_g[i] = 0;
	for (int i = 0; i < hist_len_b; i++) hist_b[i] = 0;
	
	// Fill out each histogram.
	printf("Filling Histograms...\n");
	for (int i = 0; i < width*height; i++) {
		hist_r[cnt_r.cnt[i] / histogram_scale]++;
		hist_g[cnt_g.cnt[i] / histogram_scale]++;
		hist_b[cnt_b.cnt[i] / histogram_scale]++;
	}
	
	printf("Converting to an image...\n");
	
	// Allocate the image.
	uint8_t* img = new uint8_t[width*height*3];
	if (img == NULL) {
		printf("Error allocating image.\n");
		exit(1);
	}
	
	// Paint the points.
	for (int i = 0; i < width*height; i++) {
		if (i % 100000 == 0) {
			printf("%d...\n", i);
		}
		
		float red = 0;
		float grn = 0;
		float blu = 0;
		
//		printf("Painting %d: (%d, %d, %d)\n", i, cnt_r.cnt[i], cnt_g.cnt[i], cnt_b.cnt[i]);
		
		for (int j = 0; j < cnt_r.cnt[i]; j++) {
			red += (float) hist_r[j] / (width*height);
		}
		
		for (int j = 0; j < cnt_g.cnt[i]; j++) {
			grn += (float) hist_g[j] / (width*height);
		}
		
		for (int j = 0; j < cnt_b.cnt[i]; j++) {
			blu += (float) hist_b[j] / (width*height);
		}
		
//		printf("%.2f\n", val);
		
		img[i*3  ] = red * 255;
		img[i*3+1] = grn * 255;
		img[i*3+2] = blu * 255;
	}
	
	printf("Saving...\n");
	
	// Save the img to file.
	char hdr[64];
	int hdr_len = sprintf(hdr, "P6 %d %d 255 ", width, height);
	
	FILE* fout = fopen("out.ppm", "wb");
	fwrite(hdr, 1, hdr_len, fout);
	fwrite(img, 1, width*height*3, fout);
	fclose(fout);
	
	delete[] real;
	delete[] imag;
	
	delete[] hist_r;
	delete[] hist_g;
	delete[] hist_b;
	
	delete[] img;
	
	printf("Goodbye!\n");
	
	return 0;
}