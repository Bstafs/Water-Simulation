#include "Vector3.h"

Vector3::Vector3() //constructor
{
	x = 0;
	y = 0;
	z = 0;
}

Vector3::Vector3(float x1, float y1, float z1) //construct with values.
{
	x = x1;
	y = y1;
	z = z1;
}

//The copy constructor is required for the same reason as above.
Vector3::Vector3(const Vector3& vec)
{
	x = vec.x;
	y = vec.y;
	z = vec.z;
}

//Addition
Vector3 Vector3 ::operator+(const Vector3& vec)
{
   return Vector3(x + vec.x, y + vec.y, z + vec.z);
}

Vector3& Vector3 ::operator+=(const Vector3& vec)
{
	x += vec.x;
	y += vec.y;
	z += vec.z;

	return *this;
}

//Substraction//
Vector3 Vector3 ::operator-(const Vector3& vec)
{
	return Vector3(x - vec.x, y - vec.y, z - vec.z);
}

Vector3& Vector3::operator-=(const Vector3& vec)
{
	x -= vec.x;
	y -= vec.y;
	z -= vec.z;

	return *this;
}

//scalar multiplication
Vector3 Vector3 ::operator*(float value)
{
	return Vector3(x * value, y * value, z * value);
}

Vector3& Vector3::operator*=(float value)
{
	x *= value;
	y *= value;
	z *= value;

	return *this;
}

//scalar division
Vector3 Vector3 ::operator/(float value)
{
	assert(value != 0); //prevent divide by 0
	//similar to multiplication
	return Vector3(x / value, y / value, z / value);
}

Vector3& Vector3 ::operator/=(float value)
{
	assert(value != 0);
	//similar to multiplication
	x /= value;
	y /= value;
	z /= value;

	return *this;
}

Vector3& Vector3::operator=(const Vector3& vec)
{
	//similar to addition
	x = vec.x;
	y = vec.y;
	z = vec.z;

	return *this;
}

Vector3 Vector3::operator%(const Vector3& vec)
{
	return Vector3(y * vec.z - z * vec.y, z * vec.x - x * vec.z, x * vec.y - y * vec.x);
}

Vector3& Vector3::operator%=(const Vector3& vec)
{
	return *this = CrossProduct(vec);
}

inline double Vector3::Length()const
{
	return sqrt(x * x + y * y + z * z);
}

//Dot product
inline float Vector3::DotProduct(const Vector3& vec)
{
	return (x * vec.x + y * vec.y + z * vec.z);
}

//Cross Product
inline Vector3 Vector3::CrossProduct(const Vector3& vec)
{
	return Vector3(y * vec.z - z * vec.y, z * vec.x - x * vec.z, x * vec.y - y * vec.x);
}
// Vector Length
inline float Vector3::Magnitude(const Vector3& vec)
{
	return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}
// Vector Normalize
inline Vector3 Vector3::Normalization(const Vector3& vec)
{
	Vector3 vector = vec;
	double vector_length = vector.Length();

	if (vector_length > std::numeric_limits<double>::epsilon())
	{
		this->x /= vector_length;
		this->y /= vector_length;
		this->z /= vector_length;
	}

	return vector;
}
// Vector Square
inline float Vector3::Square()
{
	return x * x + y * y + z * z;
}

//Vector Distance
inline float Vector3::Distance(const Vector3& vec1, const Vector3& vec2)
{
	float ySep = vec2.y - vec1.y;
	float xSep = vec2.x - vec1.x;
	float zSep = vec2.z - vec1.z;

	return sqrt(ySep * ySep + xSep * xSep + zSep * zSep);
}

XMFLOAT3 Vector3::Vector3ToXMFLOAT3()
{
	return XMFLOAT3(x, y, z);
}
