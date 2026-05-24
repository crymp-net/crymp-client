#pragma once

#include "Matrix.h"

template<class ftype>
class matrix_vector_product_tpl
{
public:
	int nRows{};
	int nCols{};
	int istride{};
	int jstride{};
	ftype* mtxdata{};
	ftype* vecdata{};

	matrix_vector_product_tpl(int nrows, int ncols, int _istride, int _jstride, ftype* pmtxdata, ftype* pvecdata)
	{
		nRows = nrows;
		nCols = ncols;
		istride = _istride;
		jstride = _jstride;
		mtxdata = pmtxdata;
		vecdata = pvecdata;
	}
};

template<class ftype>
class vector_scalar_product_tpl
{
public:
	ftype* data{};
	int len{};
	ftype op{};

	vector_scalar_product_tpl(int ncols, ftype* pdata, ftype scalar)
	{
		len = ncols;
		data = pdata;
		op = scalar;
	}
};

template<class ftype>
class vectorn_tpl
{
public:
	ftype* data{};
	int len{};
	int flags{};

	inline static constexpr int VECN_POOL_SIZE = 256;
	inline static ftype vecn_pool[VECN_POOL_SIZE]{};
	inline static int vecn_pool_pos = 0;

	vectorn_tpl(int _len, ftype* pdata = nullptr)
	{
		len = _len;
		if (pdata)
		{
			flags = mtx_foreign_data;
			data = pdata;
		}
		else if (len < 64)
		{
			if (vecn_pool_pos + len > VECN_POOL_SIZE)
			{
				vecn_pool_pos = 0;
			}
			data = vecn_pool + vecn_pool_pos;
			vecn_pool_pos += len;
			flags = mtx_foreign_data;
		}
		else
		{
			data = new ftype[len];
			flags = 0;
		}
	}

	vectorn_tpl(vectorn_tpl& src)
	{
		flags = src.flags & mtx_foreign_data;
		data = src.data;
		len = src.len;
		src.flags |= mtx_foreign_data;
	}

	~vectorn_tpl()
	{
		if (!(flags & mtx_foreign_data))
		{
			delete[] data;
		}
	}

	vectorn_tpl& operator=(const vectorn_tpl<ftype>& src)
	{
		if (src.len != len && !(flags & mtx_foreign_data))
		{
			if (data)
			{
				delete data;
			}
			data = new ftype[src.len];
		}
		len = src.len;
		for (int i = 0; i < len; i++)
		{
			data[i] = src.data[i];
		}
		return *this;
	}

	template<class ftype1>
	vectorn_tpl& operator=(const vectorn_tpl<ftype1>& src)
	{
		if (src.len != len && !(flags & mtx_foreign_data))
		{
			if (data)
			{
				delete data;
			}
			data = new ftype[src.len];
		}
		len = src.len;
		for (int i = 0; i < len; i++)
		{
			data[i] = src.data[i];
		}
		return *this;
	}

	vectorn_tpl& operator=(const matrix_vector_product_tpl<ftype>& src)
	{
		int i, j;
		for (i = 0; i < src.nRows; i++)
		{
			for (data[i] = 0, j = 0; j < src.nCols; j++)
			{
				data[i] += src.mtxdata[i * src.istride + j * src.jstride] * src.vecdata[j];
			}
		}
		return *this;
	}

	vectorn_tpl& operator+=(const matrix_vector_product_tpl<ftype>& src)
	{
		int i, j;
		for (i = 0; i < src.nRows; i++)
		{
			for (j = 0; j < src.nCols; j++)
			{
				data[i] += src.mtxdata[i * src.istride + j * src.jstride] * src.vecdata[j];
			}
		}
		return *this;
	}

	vectorn_tpl& operator-=(const matrix_vector_product_tpl<ftype>& src)
	{
		int i, j;
		for (i = 0; i < src.nRows; i++)
		{
			for (j = 0; j < src.nCols; j++)
			{
				data[i] -= src.mtxdata[i * src.istride + j * src.jstride] * src.vecdata[j];
			}
		}
		return *this;
	}

	vectorn_tpl& operator=(const vector_scalar_product_tpl<ftype>& src)
	{
		for (int i = 0; i < src.len; i++)
		{
			data[i] = src.data[i];
		}
		return *this;
	}
	vectorn_tpl& operator+=(const vector_scalar_product_tpl<ftype>& src)
	{
		for (int i = 0; i < src.len; i++)
		{
			data[i] += src.data[i];
		}
		return *this;
	}
	vectorn_tpl& operator-=(const vector_scalar_product_tpl<ftype>& src)
	{
		for (int i = 0; i < src.len; i++)
		{
			data[i] -= src.data[i];
		}
		return *this;
	}

	ftype len2()
	{
		ftype res = 0;
		for (int i = 0; i < len; i++)
		{
			res += data[i] * data[i];
		}
		return res;
	}

	ftype& operator[](int idx) const { return data[idx]; }

	operator ftype*() { return data; }

	vectorn_tpl& zero()
	{
		for (int i = 0; i < len; i++)
		{
			data[i] = 0;
		}
		return *this;
	}
	vectorn_tpl& allocate()
	{
		if (flags & mtx_foreign_data)
		{
			ftype* newdata = new ftype[len];
			for (int i = 0; i < len; i++)
			{
				newdata[i] = data[i];
			}
			data = newdata;
			flags &= ~mtx_foreign_data;
		}
	}

	vectorn_tpl& operator*=(ftype op)
	{
		for (int i = 0; i < len; i++)
		{
			data[i] *= op;
		}
		return *this;
	}
};

template<class ftype1, class ftype2>
ftype1 operator*(const vectorn_tpl<ftype1>& op1, const vectorn_tpl<ftype2>& op2)
{
	ftype1 res = 0;
	for (int i = 0; i < op1.len; i++)
	{
		res += op1.data[i] * op2.data[i];
	}
	return res;
}

template<class ftype1, class ftype2>
vectorn_tpl<ftype1> operator-(const vectorn_tpl<ftype1>& op1, const vectorn_tpl<ftype2>& op2)
{
	vectorn_tpl<ftype1> res(op1.len);
	for (int i = 0; i < op1.len; i++)
	{
		res.data[i] = op1.data[i] - op2.data[i];
	}
	return res;
}

template<class ftype1, class ftype2>
vectorn_tpl<ftype1>& operator+=(vectorn_tpl<ftype1>& op1, const vectorn_tpl<ftype2>& op2)
{
	for (int i = 0; i < op1.len; i++)
	{
		op1.data[i] += op2.data[i];
	}
	return op1;
}

template<class ftype1, class ftype2>
vectorn_tpl<ftype1>& operator-=(vectorn_tpl<ftype1>& op1, const vectorn_tpl<ftype2>& op2)
{
	for (int i = 0; i < op1.len; i++)
	{
		op1.data[i] -= op2.data[i];
	}
	return op1;
}

template<class ftype>
matrix_vector_product_tpl<ftype> operator*(const matrix_tpl<ftype>& mtx, const vectorn_tpl<ftype>& vec)
{
	return matrix_vector_product_tpl<ftype>(mtx.nRows, mtx.nCols, mtx.nCols, 1, mtx.data, vec.data);
}

template<class ftype>
matrix_vector_product_tpl<ftype> operator*(const vectorn_tpl<ftype>& vec, const matrix_tpl<ftype>& mtx)
{
	return matrix_vector_product_tpl<ftype>(mtx.nCols, mtx.nRows, 1, mtx.nRows, mtx.data, vec.data);
}

template<class ftype>
vector_scalar_product_tpl<ftype> operator*(const vectorn_tpl<ftype>& vec, ftype op)
{
	return vector_scalar_product_tpl<ftype>(vec.len, vec.data, op);
}

using vectorn = vectorn_tpl<double>;
using vectornf = vectorn_tpl<float>;
