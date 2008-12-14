#pragma once

#include "math3.h"
#include "MS3D_ASCII.h"


class Vis
{
public:
	math3::Vec3d pos, goalPos, velocity;
	Model *model;
	double swimDirAngle;
	double desired_speed;
	double speed;
	double turn_speed;
	double bending;
	double pitch;

	double scale;

	double myWaitTime;

	double wiggle_phase, wiggle_amplitude;

public:
	//enum visModel{model1, model2} //this right?:S
	Vis(Model *model, double scale); //hihi
	~Vis(void);

	void Update(double dt);
	void Draw();
	void newGoal();
};

void TestDrawAquarium();

float my_random();
