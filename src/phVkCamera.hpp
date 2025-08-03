// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Camera Class

#pragma once

#include "vec.hpp"
#include "mat.hpp"

// TODO: Add support for roll
// TODO: Add support for field of view and zoom
// TODO: MAYBE add function to generate perspective projection matrix? / view + perspective? (already added perspective?)
// TODO: Switch to Quaternions to avoid Gimbal Lock? (N/A?)

template <typename T = float>
struct phVkAutoCamConfig
{
	std::function<Mat4<T>(unsigned int)> getExtTarget = nullptr;	// Callback for external follow target
	std::function<Mat4<T>(unsigned int)> getExtParent = nullptr;	// Callback for external camera parent
	unsigned int target_index = 0;	// Index for external target obj
	unsigned int parent_index = 0;	// Index for external parent obj
	Vec3<T> target_offset;		// Offset from external target position
	Vec3<T> parent_offset;		// Offset from external parent position
	Vec3<T> follow_vector;		// Vector from follow target to camera - if zero, follow is look only
	Vec3<T> target_up;			// Netural up vector for target, to be transformed if roll_follow = true
	bool follow = false;		// If true, camera will follow the target
	bool look_follow = false;	// If true, camera will look at the target, even if follow = false
	bool roll_follow = false;	// If true, camera will roll to match follow target's roll
	bool parent_track = false;	// If true, camera position will track parent (overrides follow)
	T target_range = 0;			// Slop range for target (+/- distance from target position)
	T dolly_range = 0;			// Slop range for dolly (+/- distance along follow_vector)
	T pan_range = 0;			// Slop range for pan (+/-radians, relative to up vector)
	T tilt_range = 0;			// Slop range for tilt (+/-radians, relative to up vector)
	T roll_range = 0;			// Slop range for roll (+/-radians, relative to up vector)
	T target_speed = 0;			// Max target error correction speed (+/- units per second)
	T dolly_speed = 0;			// Max dolly error correction speed (+/- units per second)
	T pan_speed = 0;			// Max pan error correction speed (+/- radians per second)
	T tilt_speed = 0;			// Max tilt error correction speed (+/- radians per second)
	T roll_speed = 0;			// Max roll error correction speed (+/- radians per second)
};


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
	phVkAutoCamConfig<T> auto_cam;
	bool auto_cam_enable = false;
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


	// Automatic Camera Movement
	void setupAutoCam(phVkAutoCamConfig<T> config) { auto_cam = config; }
	void startAutoCam() { auto_cam_enable = true; }
	void stopAutoCam() { auto_cam_enable = false; }
	void update(T dt);
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

// Update auto camera (follow / parent)
template <typename T>
void phVkCamera<T>::update(T dt)
{
	if (!auto_cam_enable)
		return;

	Mat4<T> ext_target;
	Mat4<T> ext_parent;

	if (auto_cam.getExtTarget)
		ext_target = auto_cam.getExtTarget(auto_cam.target_index);

	if (auto_cam.getExtParent)
		ext_parent = auto_cam.getExtParent(auto_cam.parent_index);

	// Look Follow
	if (auto_cam.look_follow || auto_cam.follow)
	{
		Vec3<T> target_vector = ext_target.getTransl() + 
			ext_target.getSub() * auto_cam.target_offset - target;
		if (auto_cam.target_range == 0)
			target += target_vector;
		else
		{
			T target_error = target_vector.len();
			T correction = 0;

			if (target_error > abs(auto_cam.target_range))
				correction += target_error - abs(auto_cam.target_range);

			if (min(target_error, abs(auto_cam.target_range)) > abs(auto_cam.target_speed) * dt)
				correction += abs(auto_cam.target_speed) * dt;
			else
				correction += min(target_error, abs(auto_cam.target_range));

			target += target_vector.norm() * correction;
		}
	}

	// Parent Track
	if (auto_cam.parent_track && auto_cam.getExtParent)
	{
		pos = ext_parent.getTransl() + ext_parent.getSub() * auto_cam.parent_offset;
		
		// TODO: add a way to choose whether the target remains fixed (or follows the target obj),
		//		 or whether it revolves based on the rotation of the parent object
	    // TODO: could consider adding roll (up) follow to this as an option
	}
	// Target Follow
	else if (auto_cam.follow && auto_cam.getExtTarget)
	{
		// Up Vector / Roll Follow
		if (auto_cam.roll_follow)
			up_world = ext_target.getSub() * auto_cam.target_up.norm();
		else
			up_world = auto_cam.target_up.norm();	// TBD

		// Dolly Follow (along separation vector)
		Vec3<T> separation = pos - target;
		if (auto_cam.dolly_range == 0)
			separation = separation.norm() * auto_cam.follow_vector.len();
		else
		{
			T dolly_error = auto_cam.follow_vector.len() - separation.len();
			T correction = 0;

			if (abs(dolly_error) > 0)
			{
				if (abs(dolly_error) > abs(auto_cam.dolly_range))
					correction += abs(dolly_error) - abs(auto_cam.dolly_range);

				if (min(abs(dolly_error), abs(auto_cam.dolly_range)) > abs(auto_cam.dolly_speed) * dt)
					correction += abs(auto_cam.dolly_speed) * dt;
				else
					correction += min(abs(dolly_error), abs(auto_cam.dolly_range));

				separation += separation.norm() * correction * dolly_error / abs(dolly_error);
			}
		}

		Vec3<T> follow = ext_target.getSub() * auto_cam.follow_vector;
		if (follow * up_world < 0)
			follow += 2 * abs(follow * up_world) * up_world;	// Correct for up direction
		Vec3<T> pan_axis = up_world;
		Vec3<T> tilt_axis = (follow % pan_axis).norm();

		// Pan Follow
		T pan_error = Vec3<T>::angle(follow - pan_axis * (follow * pan_axis),
			separation - pan_axis * (separation * pan_axis));	// Projected onto pan_axis rotation plane
		if (auto_cam.pan_range != 0)
		{
			T correction = 0;

			if (pan_error > abs(auto_cam.pan_range))
				correction += pan_error - abs(auto_cam.pan_range);

			if (min(pan_error, abs(auto_cam.pan_range)) > abs(auto_cam.pan_speed) * dt)
				correction += abs(auto_cam.pan_speed) * dt;
			else
				correction += min(pan_error, abs(auto_cam.pan_range));

			pan_error = correction;
		}
		// Check sign of angle
		if ((follow % separation) * pan_axis >= 0)
			pan_error = -abs(pan_error);
		else
			pan_error = abs(pan_error);

		// Tilt Follow
		T tilt_error = Vec3<T>::angle(follow - tilt_axis * (follow * tilt_axis),
			separation - tilt_axis * (separation * tilt_axis));	// Projected onto tilt_axis rotation plane
		if (auto_cam.tilt_range != 0)
		{
			T correction = 0;

			if (tilt_error > abs(auto_cam.tilt_range))
				correction += tilt_error - abs(auto_cam.tilt_range);

			if (min(tilt_error, abs(auto_cam.tilt_range)) > abs(auto_cam.tilt_speed) * dt)
				correction += abs(auto_cam.tilt_speed) * dt;
			else
				correction += min(tilt_error, abs(auto_cam.tilt_range));

			tilt_error = correction;
		}
		// Check sign of angle
		if ((follow % separation) * tilt_axis >= 0)
			tilt_error = -abs(tilt_error);
		else
			tilt_error = abs(tilt_error);

		// Apply Pan and Tilt Corrections
		// TODO: can this be done more efficiently by combining rotation vectors?
		separation = Mat3<T>::rot(tilt_error, tilt_axis) * 
			Mat3<T>::rot(pan_error, pan_axis) * separation;

		pos = target + separation;
	}

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

