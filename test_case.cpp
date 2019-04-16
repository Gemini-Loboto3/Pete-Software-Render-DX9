#include <cstdio>
#include <cstdlib>

struct Matrix4f {
	/* Initialize matrix to the identity matrix */
	Matrix4f()
		: r0c0(1.0f), r0c1(0.0f), r0c2(0.0f), r0c3(0.0f)
		, r1c0(0.0f), r1c1(1.0f), r1c2(0.0f), r1c3(0.0f)
		, r2c0(0.0f), r2c1(0.0f), r2c2(1.0f), r2c3(0.0f)
		, r3c0(0.0f), r3c1(0.0f), r3c2(0.0f), r3c3(1.0f)
	{
	}

	/* Initialize matrix to row and column values */
	Matrix4f(
		float row0col0, float row0col1, float row0col2, float row0col3,
		float row1col0, float row1col1, float row1col2, float row1col3,
		float row2col0, float row2col1, float row2col2, float row2col3,
		float row3col0, float row3col1, float row3col2, float row3col3
	) noexcept
		: r0c0(row0col0), r0c1(row0col1), r0c2(row0col2), r0c3(row0col3)
		, r1c0(row1col0), r1c1(row1col1), r1c2(row1col2), r1c3(row1col3)
		, r2c0(row2col0), r2c1(row2col1), r2c2(row2col2), r2c3(row2col3)
		, r3c0(row3col0), r3c1(row3col1), r3c2(row3col2), r3c3(row3col3)
	{
	}

	/* Multiply one matrix by another */
	Matrix4f operator * (const Matrix4f& m) const {
		return Matrix4f(
			r0c0 * m.r0c0 + r0c1 * m.r1c0 + r0c2 * m.r2c0 + r0c3 * m.r3c0,  // R0C0
			r0c0 * m.r0c1 + r0c1 * m.r1c1 + r0c2 * m.r2c1 + r0c3 * m.r3c1,  // R0C1
			r0c0 * m.r0c2 + r0c1 * m.r1c2 + r0c2 * m.r2c2 + r0c3 * m.r3c2,  // R0C2
			r0c0 * m.r0c3 + r0c1 * m.r1c3 + r0c2 * m.r2c3 + r0c3 * m.r3c3,  // R0C3

			r1c0 * m.r0c0 + r1c1 * m.r1c0 + r1c2 * m.r2c0 + r1c3 * m.r3c0,  // R1C0
			r1c0 * m.r0c1 + r1c1 * m.r1c1 + r1c2 * m.r2c1 + r1c3 * m.r3c1,  // R1C1
			r1c0 * m.r0c2 + r1c1 * m.r1c2 + r1c2 * m.r2c2 + r1c3 * m.r3c2,  // R1C2
			r1c0 * m.r0c3 + r1c1 * m.r1c3 + r1c2 * m.r2c3 + r1c3 * m.r3c3,  // R1C3

			r2c0 * m.r0c0 + r2c1 * m.r1c0 + r2c2 * m.r2c0 + r2c3 * m.r3c0,  // R2C0
			r2c0 * m.r0c1 + r2c1 * m.r1c1 + r2c2 * m.r2c1 + r2c3 * m.r3c1,  // R2C1
			r2c0 * m.r0c2 + r2c1 * m.r1c2 + r2c2 * m.r2c2 + r2c3 * m.r3c2,  // R2C2
			r2c0 * m.r0c3 + r2c1 * m.r1c3 + r2c2 * m.r2c3 + r2c3 * m.r3c3,  // R2C3

			r3c0 * m.r0c0 + r3c1 * m.r1c0 + r3c2 * m.r2c0 + r3c3 * m.r3c0,  // R3C0
			r3c0 * m.r0c1 + r3c1 * m.r1c1 + r3c2 * m.r2c1 + r3c3 * m.r3c1,  // R3C1
			r3c0 * m.r0c2 + r3c1 * m.r1c2 + r3c2 * m.r2c2 + r3c3 * m.r3c2,  // R3C2
			r3c0 * m.r0c3 + r3c1 * m.r1c3 + r3c2 * m.r2c3 + r3c3 * m.r3c3   // R3C3
		);
	}

	float r0c0; float r0c1; float r0c2; float r0c3;
	float r1c0; float r1c1; float r1c2; float r1c3;
	float r2c0; float r2c1; float r2c2; float r2c3;
	float r3c0; float r3c1; float r3c2; float r3c3;
};

int check(float value, float expected)
{
	// failed
	if (value != expected) return 1;
	// ok
	return 0;
}

int checkIdentityMatrix(const Matrix4f& m)
{
	int test = 0;

	test |= check(m.r0c0, 1);
	test |= check(m.r0c1, 0);
	test |= check(m.r0c2, 0);
	test |= check(m.r0c3, 0);

	test |= check(m.r1c0, 0);
	test |= check(m.r1c1, 1);
	test |= check(m.r1c2, 0);
	test |= check(m.r1c3, 0);

	test |= check(m.r2c0, 0);
	test |= check(m.r2c1, 0);
	test |= check(m.r2c2, 1);
	test |= check(m.r2c3, 0);

	test |= check(m.r3c0, 0);
	test |= check(m.r3c1, 0);
	test |= check(m.r3c2, 0);
	test |= check(m.r3c3, 1);

	return test;
}

int Test_instruction_set()
{

	Matrix4f mat1;
	if (checkIdentityMatrix(mat1))
		return 0;

	Matrix4f mat2;
	if (checkIdentityMatrix(mat2))
		return 0;

	Matrix4f mat3 = mat1 * mat2;
	if (checkIdentityMatrix(mat3))
		return 0;

	return 1;
}
