#pragma once

#include "CryCommon/CryMath/Cry_Math.h"

template<class ftype>
class quotient_tpl
{
public:
	ftype x{};
	ftype y{};

	quotient_tpl() = default;

	explicit quotient_tpl(ftype x, ftype y = 1) : x(x), y(y) {}

	quotient_tpl(const quotient_tpl&) = default;

	template<class ftype1>
	quotient_tpl(const quotient_tpl<ftype1>& src) : x(src.x), y(src.y)
	{
	}

	template<class ftype1, class ftype2>
	quotient_tpl& set(ftype1 nx, ftype2 ny)
	{
		(*this = nx) /= ny;
		return *this;
	}

	quotient_tpl& operator=(const quotient_tpl&) = default;

	template<class ftype1>
	quotient_tpl& operator=(const quotient_tpl<ftype1>& src)
	{
		x = src.x;
		y = src.y;
		return *this;
	}

	quotient_tpl& operator=(ftype src)
	{
		x = src;
		y = 1;
		return *this;
	}

	quotient_tpl& fixsign()
	{
		int sgny = ::sgnnz(y);
		x *= sgny;
		y *= sgny;
		return *this;
	}

	ftype val() { return y != 0 ? x / y : 0; }

	quotient_tpl operator-() const { return quotient_tpl(-x, y); }

	quotient_tpl operator*(ftype op) const { return quotient_tpl(x * op, y); }
	quotient_tpl operator/(ftype op) const { return quotient_tpl(x, y * op); }
	quotient_tpl operator+(ftype op) const { return quotient_tpl(x + y * op, y); }
	quotient_tpl operator-(ftype op) const { return quotient_tpl(x - y * op, y); }

	quotient_tpl& operator*=(ftype op)
	{
		x *= op;
		return *this;
	}

	quotient_tpl& operator/=(ftype op)
	{
		y *= op;
		return *this;
	}

	quotient_tpl& operator+=(ftype op)
	{
		x += op * y;
		return *this;
	}

	quotient_tpl& operator-=(ftype op)
	{
		x -= op * y;
		return *this;
	}

	bool operator==(ftype op) const { return x == op * y; }
	bool operator!=(ftype op) const { return x != op * y; }
	bool operator<(ftype op) const { return x - op * y < 0; }
	bool operator>(ftype op) const { return x - op * y > 0; }
	bool operator<=(ftype op) const { return x - op * y <= 0; }
	bool operator>=(ftype op) const { return x - op * y >= 0; }

	int sgn() { return ::sgn(x); }
	int sgnnz() { return ::sgnnz(x); }
	int isneg() { return ::isneg(x); }
	int isnonneg() { return ::isnonneg(x); }
	int isin01() { return ::isneg(::fabs_tpl(x * 2 - y) - ::fabs_tpl(y)); }

	template<class ftype2>
	friend quotient_tpl operator*(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return quotient_tpl(op1.x * op2.x, op1.y * op2.y);
	}

	template<class ftype2>
	friend quotient_tpl operator/(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return quotient_tpl(op1.x * op2.y, op1.y * op2.x);
	}

	template<class ftype2>
	friend quotient_tpl operator+(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return quotient_tpl(op1.x * op2.y + op2.x * op1.y, op1.y * op2.y);
	}

	template<class ftype2>
	friend quotient_tpl operator-(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return quotient_tpl(op1.x * op2.y - op2.x * op1.y, op1.y * op2.y);
	}

	template<class ftype2>
	friend quotient_tpl& operator*=(quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		op1.x *= op2.x;
		op1.y *= op2.y;
		return op1;
	}

	template<class ftype2>
	friend quotient_tpl& operator/=(quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		op1.x *= op2.y;
		op1.y *= op2.x;
		return op1;
	}

	template<class ftype2>
	friend quotient_tpl& operator+=(quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		op1.x = op1.x * op2.y + op2.x * op1.y;
		op1.y *= op2.y;
		return op1;
	}

	template<class ftype2>
	friend quotient_tpl& operator-=(quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		op1.x = op1.x * op2.y - op2.x * op1.y;
		op1.y *= op2.y;
		return op1;
	}

	friend quotient_tpl operator*(ftype op, const quotient_tpl& q) { return quotient_tpl(q.x * op, q.y); }
	friend quotient_tpl operator/(ftype op, const quotient_tpl& q) { return quotient_tpl(q.x, q.y * op); }
	friend quotient_tpl operator+(ftype op, const quotient_tpl& q) { return quotient_tpl(op * q.y + q.x, q.y); }
	friend quotient_tpl operator-(ftype op, const quotient_tpl& q) { return quotient_tpl(op * q.y - q.x, q.y); }

	friend bool operator==(ftype op1, const quotient_tpl& op2) { return (op1 * op2.y) == op2.x; }
	friend bool operator!=(ftype op1, const quotient_tpl& op2) { return (op1 * op2.y) != op2.x; }
	friend bool operator<(ftype op1, const quotient_tpl& op2) { return (op1 * op2.y - op2.x) < 0; }
	friend bool operator>(ftype op1, const quotient_tpl& op2) { return (op1 * op2.y - op2.x) > 0; }
	friend bool operator<=(ftype op1, const quotient_tpl& op2) { return (op1 * op2.y - op2.x) <= 0; }
	friend bool operator>=(ftype op1, const quotient_tpl& op2) { return (op1 * op2.y - op2.x) >= 0; }

	template<class ftype2>
	friend bool operator==(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return op1.x * op2.y == op2.x * op1.y;
	}

	template<class ftype2>
	friend bool operator!=(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return op1.x * op2.y != op2.x * op1.y;
	}

	template<class ftype2>
	friend bool operator<(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return op1.x * op2.y - op2.x * op1.y + ::iszero(op1.y + op2.y) * (op1.x - op1.y) < 0;
	}

	template<class ftype2>
	friend bool operator>(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return op1.x * op2.y - op2.x * op1.y + ::iszero(op1.y + op2.y) * (op1.x - op1.y) > 0;
	}

	template<class ftype2>
	friend bool operator<=(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return op1.x * op2.y - op2.x * op1.y + ::iszero(op1.y + op2.y) * (op1.x - op1.y) <= 0;
	}

	template<class ftype2>
	friend bool operator>=(const quotient_tpl& op1, const quotient_tpl<ftype2>& op2)
	{
		return op1.x * op2.y - op2.x * op1.y + ::iszero(op1.y + op2.y) * (op1.x - op1.y) >= 0;
	}

	friend int sgn(const quotient_tpl& op) { return ::sgn(op.x); }
	friend int sgnnz(const quotient_tpl& op) { return ::sgnnz(op.x); }
	friend int isneg(const quotient_tpl& op) { return ::isneg(op.x); }
	friend int isnonneg(const quotient_tpl& op) { return ::isnonneg(op.x); }
	friend int sgn_safe(const quotient_tpl& op) { return ::sgn(op.x) * ::sgnnz(op.y); }
	friend int sgnnz_safe(const quotient_tpl& op) { return ::sgnnz(op.x) * ::sgnnz(op.y); }
	friend int isneg_safe(const quotient_tpl& op) { return ::isneg(op.x) ^ ::isneg(op.y); }
	friend int isnonneg_safe(const quotient_tpl& op) { return ::isnonneg(op.x) * ::isnonneg(op.y); }

	friend quotient_tpl fabs_tpl(const quotient_tpl& op)
	{
		return quotient_tpl(::fabs_tpl(op.x), ::fabs_tpl(op.y));
	}

	friend quotient_tpl max(const quotient_tpl& op1, const quotient_tpl& op2) { return op1 > op2 ? op1 : op2; }
	friend quotient_tpl min(const quotient_tpl& op1, const quotient_tpl& op2) { return op1 < op2 ? op1 : op2; }
};

template<class ftype>
quotient_tpl<ftype> fake_atan2(ftype y, ftype x)
{
	quotient_tpl<ftype> res;
	ftype src[2] = {x, y};
	int ix = isneg(x), iy = isneg(y), iflip = isneg(fabs_tpl(x) - fabs_tpl(y));
	res.x = src[iflip ^ 1] * (1 - (iflip * 2)) * sgnnz(src[iflip]);
	res.y = fabs_tpl(src[iflip]);
	res += (iy * 2 + (ix ^ iy) + (iflip ^ ix ^ iy)) * 2;
	return res;
}

using quotient = quotient_tpl<double>;
using quotientf = quotient_tpl<float>;
using quotienti = quotient_tpl<int>;
