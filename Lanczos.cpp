#include "stdafx.h"
#include <cmath>
#include <vector>

double lanczos_size_ = 3.0;
inline double sinc(double x)
{
	double pi = 3.1415926;
	x = (x * pi);
	if (x < 0.01 && x > -0.01)
		return 1.0 + x * x*(-1.0 / 6.0 + x * x*1.0 / 120.0);
	return sin(x) / x;
}

inline double LanczosFilter(double x)
{
	if (abs(x) < lanczos_size_)
	{
		double pi = 3.1415926;
		return sinc(x)*sinc(x / lanczos_size_);
	}
	return 0.0;
}

class FloatMatrix
{
public:
	class Row
	{
		friend class FloatMatrix;
	public:
		double& operator[](int col)
		{
			return parent.data[row][col];
		}
		Row(FloatMatrix &parent_, int row_) : parent(parent_), row(row_) {}

		FloatMatrix& parent;
		int row;
	};
	Row operator[](int row) { return Row(*this, row); }

	FloatMatrix(int w, int h)
	{
		data.resize(h);
		//data = new double*[h];
		for (int i = 0; i < h; i++)
			data[i].resize(w * 3);
		//	data[i] = new double[w * 3];
		width = w;
		height = h;
	}
	~FloatMatrix()
	{
		data.clear();
		//for (int i = 0; i < height; i++)
		//	delete[] data[i];
		//delete[] data;
	}

	double *GetAt(int x, int y) { return &data[y][x * 3]; }

	//float operator[](int x) { return data[x]; }
	//float operator[](int x, int y) { return data[y*width+x]; }

protected:
	int width, height;
	//double **data;
	std::vector<std::vector<double>> data;
};

void ResizeLanczos(unsigned char *img, int pitch, int src_w, int src_h, unsigned char *dst, int new_width, int new_height)
{
	int old_cols = src_w;
	int old_rows = src_h;
	double col_ratio = (double)(old_cols) / (double)(new_width);
	double row_ratio = (double)(old_rows) / (double)(new_height);

	// Apply filter first in width, then in height.
	FloatMatrix horiz_image(new_width, old_rows);
	for (int r = 0; r < old_rows; r++)
	{
		for (int c = 0; c < new_width; c++)
		{
			// x is the new col in terms of the old col coordinates.
			double x = (double)(c)*col_ratio;
			// The old col corresponding to the closest new col.
			int floor_x = static_cast<int>(x);

			double *hh = horiz_image.GetAt(c, r);

			hh[0] = 0.f;
			hh[1] = 0.f;
			hh[2] = 0.f;
			double weight = 0.0;
			// Add up terms across the filter.
			for (int i = (int)(floor_x - lanczos_size_ + 1); i <= floor_x + lanczos_size_; i++)
			{
				if (i >= 0 && i < old_cols)
				{
					unsigned char *row = &img[pitch * r + i * 4];
					double lanc_term = LanczosFilter(x - i);
					//p1=img.GetPixelIndex(i,r);
					//RGBQUAD qi=*(RGBQUAD*)&p1;
					hh[0] += ((double)row[2] / 255.0 * lanc_term);
					hh[1] += ((double)row[1] / 255.0 * lanc_term);
					hh[2] += ((double)row[0] / 255.0 * lanc_term);
					weight += lanc_term;
				}
			}

			for (int i = 0; i < 3; i++)
			{
				// Normalize the filter.
				hh[i] = (hh[i] / weight);
				// Strap the pixel values to valid values.
				hh[i] = (hh[i] > 1.0f) ? 1.0f : hh[i];
				hh[i] = (hh[i] < 0.0f) ? 0.0f : hh[i];
			}
		}
	}

	// Now apply a vertical filter to the horiz image.
	//Image new_image(new_cols, new_rows);
	FloatMatrix new_image(new_width, new_height);
	for (int r = 0; r < new_height; r++)
	{
		double x = static_cast<double>(r)*row_ratio;
		int floor_x = static_cast<int>(x);
		for (int c = 0; c < new_width; c++)
		{
			double *hn = new_image.GetAt(c, r);
			hn[0] = 0.f;
			hn[1] = 0.f;
			hn[2] = 0.f;
			double weight = 0.0;
			for (int i = (int)(floor_x - lanczos_size_ + 1); i <= floor_x + lanczos_size_; i++)
			{
				if (i >= 0 && i < old_rows)
				{
					double *h0 = horiz_image.GetAt(c, i);
					double lanc_term = LanczosFilter(x - i);
					hn[0] += (h0[0] * lanc_term);
					hn[1] += (h0[1] * lanc_term);
					hn[2] += (h0[2] * lanc_term);
					weight += lanc_term;
				}
			}

			for (int i = 0; i < 3; i++)
			{
				hn[i] = (hn[i] / weight);
				hn[i] = (hn[i] > 1.0f) ? 1.0f : hn[i];
				hn[i] = (hn[i] < 0.0f) ? 0.0f : hn[i];
			}
			dst[2] = hn[0] * 255.f;
			dst[1] = hn[1] * 255.f;
			dst[0] = hn[2] * 255.f;
			dst += 3;
		}
	}
}
