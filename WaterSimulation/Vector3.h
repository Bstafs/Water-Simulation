#pragma once

#include<iostream>
#include<math.h>
#include<assert.h>
#include <directxmath.h>
#include <d3d11_1.h>

using namespace std;
using namespace DirectX;

class Vector3
{
public:
	//Variables
	float x, y, z;

	//Constructors
	Vector3(); //constructor
	Vector3(float x1, float y1, float z1 = 0); //construct with values.
	Vector3(const Vector3& vec); //copy constructor

   //Arithemetic Operators – note use of overloading
	Vector3 operator+(const Vector3& vec); //addition
	Vector3& operator+=(const Vector3& vec);//assign new result to vector
	Vector3 operator-(const Vector3& vec); //substraction
	Vector3& operator-=(const Vector3& vec);//assign new result to vector
	Vector3 operator*(float value); //multiplication
	Vector3& operator*=(float value); //assign new result to vector.
	Vector3 operator/(float value); //division
	Vector3& operator/=(float value); //assign new result to vector
	Vector3& operator=(const Vector3& vec);
	Vector3 operator%(const Vector3& vec);
	Vector3& operator%=(const Vector3& vec);

	//
	inline double Length()const;
	XMFLOAT3 Vector3ToXMFLOAT3();

	//Vector operations
    inline float DotProduct(const Vector3& vec); //scalar dot_product
	inline Vector3 CrossProduct(const Vector3& vec);
	inline Vector3 Normalization(const Vector3& vec); //normalized vector

	//Scalar Operations
	inline float Square(); //gives square of the vector
	inline float Distance(const Vector3& vec1, const Vector3& vec2); //distance between two vectors
	inline float Magnitude(const Vector3& vec); //magnitude of the vector
	
	float Magnitude() { return sqrt(x * x + y * y + z * z); }

	Vector3 Normalize()
	{
		if (x == 0 && y == 0 && z == 0)
		{
			return Vector3();
		}
		float norm = 1 / Magnitude();
		return Vector3(norm * x, norm * y, norm * z);
	}
};