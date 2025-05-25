// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Camera Class

#pragma once

#include "vec.hpp"
#include "mat.hpp"

// TODO: Add support for roll
// TODO: Add support for field of view and zoom
// TODO: MAYBE add function to generate perspective projection matrix? / view + perspective?
// TODO: Switch to Quaternions to avoid Gimbal Lock?

template <typename T = float>
class phVkCamera
{
private:
	Vec3<T> pos;				// Position of camera in world space
	Vec3<T> target;				// Target position in world space
	Vec3<T> up_world;			// 'Up' axis vector in world space
	Vec3<T> cache_cam_dir;		// Cache of camera 'direction' vector / z-axis (pos - target).norm()
	Vec3<T> cache_cam_right;	// Cache of camera 'right' vector / x-axis (normalized)
	Vec3<T> cache_cam_up;		// Cache of camera 'up' vector / y-axis (normalized)
	Mat4<T> cache_lookat_mat;	// Cache of camera look-at matrix
	bool cam_dir_valid;
	bool cam_right_valid;
	bool cam_up_valid;
	bool lookat_valid;
public:
	phVkCamera();
	phVkCamera(Vec3<T> p, Vec3<T> t, Vec3<T> up = Vec3<T>(0, 1, 0));
	~phVkCamera();

	void setPos(Vec3<T> p);
	void setTarget(Vec3<T> t);
	void setUpVector(Vec3<T> up);

	Vec3<T> getPos() { return pos; }
	Vec3<T> getTarget() { return target; }
	Vec3<T> getUpVector() { return up_world; }
	Vec3<T> getCamDir();
	Vec3<T> getCamRight();
	Vec3<T> getCamUp();
	Mat4<T> getLookAt();

	void moveGlobal(Vec3<T> t);
	void movePos(Vec3<T> t);
	void revolveH(T angle);
	void revolveV(T angle);
	// 'Real' camera movements
	void truck(T x);
	void pedestal(T y);
	void dolly(T z);
	void tilt(T angle);
	void pan(T angle);
	//void roll(T angle);
	//void zoom(T zoom);
};

// ****Camera IMPLEMENTATION****

// Default Constructor
template <typename T>
phVkCamera<T>::phVkCamera()
{
	pos = Vec3<T>(0, 0, 1);
	target = Vec3<T>(0, 0, 0);
	up_world = Vec3<T>(0, 1, 0);
	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// Constructor
// p = position
// t = target
// up = up vector (world)
template <typename T>
phVkCamera<T>::phVkCamera(Vec3<T> p, Vec3<T> t, Vec3<T> up)
{
	pos = p;
	target = t;
	up_world = up;
	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// Destructor (does nothing)
template <typename T>
phVkCamera<T>::~phVkCamera()
{
	return;
}

template <typename T>
void phVkCamera<T>::setPos(Vec3<T> p)
{
	pos = p;
	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

template <typename T>
void phVkCamera<T>::setTarget(Vec3<T> t)
{
	target = t;
	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

template <typename T>
void phVkCamera<T>::setUpVector(Vec3<T> up)
{
	up_world = up;
	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// Get/Generate Camera 'Direction' / z-axis Vector
template <typename T>
Vec3<T> phVkCamera<T>::getCamDir()
{
	if (!cam_dir_valid)
	{
		cache_cam_dir = (pos - target).norm();
		cam_dir_valid = true;
	}

	return cache_cam_dir;
}

// Get/Generate Camera 'right' / x-axis Vector
template <typename T>
Vec3<T> phVkCamera<T>::getCamRight()
{
	if (!cam_right_valid)
	{
		if (!cam_dir_valid)
		{
			cache_cam_dir = (pos - target).norm();
			cam_dir_valid = true;
		}
		cache_cam_right = (up_world % cache_cam_dir).norm();
		cam_right_valid = true;
	}

	return cache_cam_right;
}

// Get/Generate Camera 'up' / y-axis Vector
template <typename T>
Vec3<T> phVkCamera<T>::getCamUp()
{
	if (!cam_up_valid)
	{
		if (!cam_dir_valid)
		{
			cache_cam_dir = (pos - target).norm();
			cam_dir_valid = true;
		}
		if (!cam_right_valid)
		{
			cache_cam_right = (up_world % cache_cam_dir).norm();
			cam_right_valid = true;
		}

		cache_cam_up = (cache_cam_dir % cache_cam_right).norm();
		cam_up_valid = true;
	}

	return cache_cam_up;
}

// Get/Generate Look-at Matrix
template <typename T>
Mat4<T> phVkCamera<T>::getLookAt()
{
	if (lookat_valid)
	{
		return cache_lookat_mat;
	}
	else
	{
		// Gram-Schmidt Process: Re-generate Parts
		if (!cam_dir_valid)
		{
			cache_cam_dir = (pos - target).norm();
			cam_dir_valid = true;
		}
		if (!cam_right_valid)
		{
			cache_cam_right = (up_world % cache_cam_dir).norm();
			cam_right_valid = true;
		}
		if (!cam_up_valid)
		{
			cache_cam_up = (cache_cam_dir % cache_cam_right).norm();
			cam_up_valid = true;
		}


		cache_lookat_mat = Mat4<T>(
			Vec4<T>(cache_cam_right.x, cache_cam_up.x, cache_cam_dir.x, 0),	// Column 1
			Vec4<T>(cache_cam_right.y, cache_cam_up.y, cache_cam_dir.y, 0),	// Column 2
			Vec4<T>(cache_cam_right.z, cache_cam_up.z, cache_cam_dir.z, 0),	// Column 3
			Vec4<T>(0, 0, 0, 1)												// Column 4
		);

		Mat4<T> temp_pos_mat;	// Identity
		temp_pos_mat[3] = Vec4<T>(-pos.x, -pos.y, -pos.z, 1);	// Column 4

		cache_lookat_mat = cache_lookat_mat * temp_pos_mat;

		lookat_valid = true;
		return cache_lookat_mat;
	}
}

// Move entire camera in global coordinates
// t = move vector
template <typename T>
void phVkCamera<T>::moveGlobal(Vec3<T> t)
{
	pos += t;
	target += t;

	lookat_valid = false;
}

// Move camera in global coordinates but don't affect target
// t = move vector
template <typename T>
void phVkCamera<T>::movePos(Vec3<T> t)
{
	pos += t;

	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// Revolve camera around target vertically
// angle = angle to revolve in radians
template <typename T>
void phVkCamera<T>::revolveV(T angle)
{
	Mat3<T> rot_mat = Mat3<T>::rot(-angle, getCamRight());
	Vec3<T> t_dir = rot_mat * getCamDir();
	pos = target + t_dir * (pos - target).len();

	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// Revolve camera around target horizontally
// angle = angle to revolve in radians
template <typename T>
void phVkCamera<T>::revolveH(T angle)
{
	Mat3<T> rot_mat = Mat3<T>::rot(angle, getCamUp());
	Vec3<T> t_dir = rot_mat * getCamDir();
	pos = target + t_dir * (pos - target).len();

	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// 'Truck' camera - move left or right along local axis
// x = distance to move
template <typename T>
void phVkCamera<T>::truck(T x)
{
	pos += getCamRight() * x;
	target += getCamRight() * x;

	lookat_valid = false;
}

// 'Pedestal' camera - move up or down along local axis
// y = distance to move
template <typename T>
void phVkCamera<T>::pedestal(T y)
{
	pos += getCamUp() * y;
	target += getCamUp() * y;

	lookat_valid = false;
}

// 'Dolly' camera - move forward or back along local axis
// z = distance to move
template <typename T>
void phVkCamera<T>::dolly(T z)
{
	pos -= getCamDir() * z;
	target -= getCamDir() * z;

	lookat_valid = false;
}

// 'Tilt' camera - rotate vertically around position
// angle = angle to rotate in radians
template <typename T>
void phVkCamera<T>::tilt(T angle)
{
	Mat3<T> rot_mat = Mat3<T>::rot(angle, getCamRight());
	Vec3<T> t_dir = rot_mat * -getCamDir();
	target = pos + t_dir * (target - pos).len();

	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// 'Pan' camera - rotate Horizontally around position
// angle = angle to rotate in radians
template <typename T>
void phVkCamera<T>::pan(T angle)
{
	Mat3<T> rot_mat = Mat3<T>::rot(-angle, getCamUp());
	Vec3<T> t_dir = rot_mat * -getCamDir();
	target = pos + t_dir * (target - pos).len();

	cam_dir_valid = cam_right_valid = cam_up_valid = lookat_valid = false;
}

// ****END IMPLEMENTATION****

// STATIC FUNCTIONS (non-class)

// Camera Static Function
// Generate look-at matrix from poition, target, and world up
// pos = position of camera
// target = look-at target
// up = up vector in world space
// Returns: look-at matrix
template <typename T>
Mat4<T> LookAt(Vec3<T> pos, Vec3<T> target, Vec3<T> up)
{
	Vec3<T> cam_dir, cam_right, cam_up;
	Mat4<T> lookat_mat;
	Mat4<T> temp_pos_mat;	// Identity

	// Gram-Schmidt Process
	cam_dir = (pos - target).norm();

	cam_right = (up % cam_dir).norm();

	cam_up = (cam_dir % cam_right).norm();

	lookat_mat = Mat4<T>(
		Vec4<T>(cam_right.x, cam_up.x, cam_dir.x, 0),	// Column 1
		Vec4<T>(cam_right.y, cam_up.y, cam_dir.y, 0),	// Column 2
		Vec4<T>(cam_right.z, cam_up.z, cam_dir.z, 0),	// Column 3
		Vec4<T>(0, 0, 0, 1)								// Column 4
	);

	temp_pos_mat[3] = Vec4<T>(-pos.x, -pos.y, -pos.z, 1);	// Column 4

	lookat_mat = lookat_mat * temp_pos_mat;

	return lookat_mat;
}

