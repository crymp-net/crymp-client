//////////////////////////////////////////////////////////////////////
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
//	
//	File:Cry_Vector2.h
//	Description: Common matrix class
//
//	History:
//	-Feb 27,2003: Created by Ivo Herzeg
//
//////////////////////////////////////////////////////////////////////

#pragma once

template<class F> struct Vec2_tpl 
{
	F x{};
	F y{};

	Vec2_tpl() = default;

	ILINE Vec2_tpl(F vx,F vy) { x=F(vx); y=F(vy);  }

	ILINE Vec2_tpl& set(F nx,F ny) { x=F(nx); y=F(ny); return *this; }


	template<class F1> ILINE Vec2_tpl(const Vec2_tpl<F1> &src) { x=F(src.x); y=F(src.y); }
	template<class F1> ILINE explicit Vec2_tpl(const Vec3_tpl<F1> &src) { x=F(src.x); y=F(src.y); }
	template<class F1> ILINE explicit Vec2_tpl(const F1 *psrc) { x=F(psrc[0]); y=F(psrc[1]); }

	explicit ILINE Vec2_tpl(const Vec3_tpl<F>& v) : x((F)v.x), y((F)v.y) { assert(this->IsValid()); }

	int operator!() const { return x==0 && y==0; }

	Vec2_tpl& Normalize() 
	{ 
		F rlen=sqrt_tpl(x*x+y*y); 	assert(rlen);
		rlen=F(1.0/rlen); 
		x*=rlen; y*=rlen; 
		return *this; 
	}

	Vec2_tpl& NormalizeSafe(const Vec2_tpl<F>& safe = {})
	{ 
		F rlen=sqrt_tpl(x*x+y*y); 
		if (rlen>0) 
		{ 
			rlen=F(1.0/rlen); 
			x*=rlen; y*=rlen; 
		} 
		else
		{
			*this = safe;
		}
		return *this; 
	}

	Vec2_tpl GetNormalized() const 
	{ 
		F rlen=sqrt_tpl(x*x+y*y);	assert(rlen);
		rlen=F(1.0/rlen); 
		return Vec2_tpl<F>(x*rlen,y*rlen); 
	}

	Vec2_tpl GetNormalizedSafe(const Vec2_tpl& safe = Vec2_tpl(1, 0)) const
	{ 
		F rlen=sqrt_tpl(x*x+y*y); 
		if (rlen>0) 
		{ 
			rlen=F(1.0/rlen); 
			return Vec2_tpl<F>(x*rlen,y*rlen); 
		} 
		return safe; 
	}
	F GetLength() const { return sqrt_tpl(x*x+y*y); }
	F GetLength2() const { return x*x+y*y; }
	F area() const { return x*y; }

	F& operator[](int idx) { return *((F*)&x+idx); }
	F operator[](int idx) const { return *((F*)&x+idx); }
	operator F*() { return &x; }
	Vec2_tpl& flip() { x=-x;y=-y; return *this; }
	Vec2_tpl& zero() { x=y=0; return *this; }
	Vec2_tpl rot90ccw() { return Vec2_tpl(-y,x); }
	Vec2_tpl rot90cw() { return Vec2_tpl(y,-x); }

	F atan2() const { return atan2_tpl(y,x); }

	Vec2_tpl operator-() const { return Vec2_tpl(-x,-y); }

	Vec2_tpl operator*(F k) const { return Vec2_tpl(x*k,y*k); }
	Vec2_tpl& operator*=(F k) { x*=k;y*=k; return *this; }
	Vec2_tpl operator/(F k) const { return *this*((F)1.0/k); }
	Vec2_tpl& operator/=(F k) { return *this*=((F)1.0/k); }
	
	bool operator==(const Vec2_tpl<F> &vec) { return x == vec.x && y == vec.y; }
	bool operator!=(const Vec2_tpl<F> &vec) { return !(*this == vec); }

  F Dot(const Vec2_tpl &rhs) const {return x*rhs.x + y*rhs.y;}
  /// returns a vector perpendicular to this one (this->Cross(newVec) points "up")
  Vec2_tpl Perp() const {return Vec2_tpl(-y, x);}

	// The size of the "paralell-trapets" area spanned by the two vectors.
	ILINE F Cross (const Vec2_tpl<F> &v) const
	{
		return float (x*v.y - y*v.x);
	}	

	/*!
	* Linear-Interpolation between Vec3 (lerp)
	* 
	* Example:
	*  Vec3 r=Vec3::CreateLerp( p, q, 0.345f );
	*/
	ILINE void SetLerp( const Vec2_tpl<F> &p, const Vec2_tpl<F> &q, F t ) {	*this = p*(1.0f-t) + q*t;}
	ILINE static Vec2_tpl<F> CreateLerp( const Vec2_tpl<F> &p, const Vec2_tpl<F> &q, F t ) {	return p*(1.0f-t) + q*t;}


	/*!
	* Spherical-Interpolation between 3d-vectors (geometrical slerp)
	* both vectors are assumed to be normalized.  
	*
	* Example:
	*  Vec3 r=Vec3::CreateSlerp( p, q, 0.5674f );
	*/
	ILINE void SetSlerp( const Vec2_tpl<F>& p, const Vec2_tpl<F>& q, F t ) {
		assert((fabs_tpl(1-(p|p)))<0.005); //check if unit-vector
		assert((fabs_tpl(1-(q|q)))<0.005); //check if unit-vector
		// calculate cosine using the "inner product" between two vectors: p*q=cos(radiant)
		F cosine = (p|q);
		//we explore the special cases where the both vectors are very close together, 
		//in which case we approximate using the more economical LERP and avoid "divisions by zero" since sin(Angle) = 0  as   Angle=0
		if(cosine>=(F)0.99) 
		{
			SetLerp(p,q,t); //perform LERP:
			this->Normalize();
		}	else 
		{
			//perform SLERP: because of the LERP-check above, a "division by zero" is not possible
			F rad				= acos_tpl(cosine);
			F scale_0   = sin_tpl((1-t)*rad);
			F scale_1   = sin_tpl(t*rad);
			*this=(p*scale_0 + q*scale_1) / sin_tpl(rad);
			this->Normalize();
		}
	}
	ILINE static Vec2_tpl<F> CreateSlerp( const Vec2_tpl<F>& p, const Vec2_tpl<F>& q, F t ) {
		Vec2_tpl<F> v;	v.SetSlerp(p,q,t); return v;	
	}

	ILINE bool IsValid() const
	{
		if (!NumberValid(x)) return false;
		if (!NumberValid(y)) return false;
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////
// Typedefs                                                                  // 
///////////////////////////////////////////////////////////////////////////////

typedef Vec2_tpl<float> Vec2;
typedef Vec2_tpl<int> Vec2i;
typedef Vec2_tpl<real> Vec2_f64;
typedef Vec2_tpl<real> Vec2r;

typedef Vec2_tpl<float> vector2f;
#if defined(LINUX64)
typedef Vec2_tpl<int>	 vector2l;
#else
typedef Vec2_tpl<long>	 vector2l;
#endif
typedef Vec2_tpl<float> vector2df;
typedef Vec2_tpl<real> vector2d;
typedef Vec2_tpl<int> vector2di;
typedef Vec2_tpl<unsigned int> vector2dui;

template<class F>
Vec2_tpl<F> operator*(F op1, const Vec2_tpl<F> &op2) {return Vec2_tpl<F>(op1*op2.x, op1*op2.y);}

template<class F1,class F2>
F1 operator*(const Vec2_tpl<F1> &op1, const Vec2_tpl<F2>& op2) { return op1.x*op2.x+op1.y*op2.y; } // dot product
template<class F1,class F2>
F1 operator|(const Vec2_tpl<F1> &op1, const Vec2_tpl<F2>& op2) { return op1.x*op2.x+op1.y*op2.y; } // dot product

template<class F1,class F2>
F1 operator^(const Vec2_tpl<F1> &op1, const Vec2_tpl<F2>& op2) { return op1.x*op2.y-op1.y*op2.x; } // cross product

template<class F1,class F2>
Vec2_tpl<F1> operator+(const Vec2_tpl<F1> &op1, const Vec2_tpl<F2> &op2) { 
	return Vec2_tpl<F1>(op1.x+op2.x,op1.y+op2.y); 
}
template<class F1,class F2>
Vec2_tpl<F1> operator-(const Vec2_tpl<F1> &op1, const Vec2_tpl<F2> &op2) { 
	return Vec2_tpl<F1>(op1.x-op2.x,op1.y-op2.y); 
}

template<class F1,class F2>
Vec2_tpl<F1>& operator+=(Vec2_tpl<F1> &op1, const Vec2_tpl<F2> &op2) { 
	op1.x+=op2.x;op1.y+=op2.y; return op1; 
}
template<class F1,class F2>
Vec2_tpl<F1>& operator-=(Vec2_tpl<F1> &op1, const Vec2_tpl<F2> &op2) { 
	op1.x-=op2.x;op1.y-=op2.y; return op1; 
}
