//-----------------------------------------------------------------------------
// app0002 - Builds with SKA Version 3.1 - Sept 01, 2012 - Michael Doherty
//-----------------------------------------------------------------------------
// AnimationControl.cpp
//    Animation controller for a single character defined by a BVH file.
//-----------------------------------------------------------------------------
// SKA configuration
#include <Core/SystemConfiguration.h>
// C/C++ libraries
#include <cstdio>
#include <complex>
// SKA modules
#include <Core/Utilities.h>
#include <Animation/MotionSequenceController.h>
#include <Animation/AnimationException.h>
#include <Animation/Skeleton.h>
#include <DataManagement/DataManager.h>
#include <DataManagement/DataManagementException.h>
// local application
#include "AppConfig.h"
#include "Connector.h"
#include "AnimationControl.h"
#include "DataInput.h"
#include "MotionGraph.h"
#include "MotionGraphController.h"
#include <vector>
using namespace std;
//#include "DataInput.h"
// global single instance of the animation controller
AnimationControl anim_ctrl;

// This example will create and animate one character.
// The character's skeleton and motion will be defined by a BVH file.

static string character_BVH("Baseball_Swings - old/swing5.bvh");

// scale the character to about 20%
// Most things in SKA were developed to fit the CMU ASF skeletons.
// The BVH files from Pacific's mocap lab are about 5 times to large 
// for this world. In particular, bones will be too thin if stretched
// to 500% of expected size, since the bone width does not currently scale.
float character_size_scale = 0.2f;

AnimationControl::AnimationControl() 
	: ready(false), run_time(0.0f), character(NULL), 
	single_step(false), freeze(false), time_warp(1.0f)
{ } 

AnimationControl::~AnimationControl()	
{ 
	if (character != NULL) delete character; 
}

bool AnimationControl::updateAnimation(float _elapsed_time)
{
	if (!ready) return false;

	// time warp simply scales the elapsed time
	_elapsed_time *= time_warp;

	if (!freeze || single_step) // 
	{
		run_time += _elapsed_time;
		if (character != NULL) character->update(run_time);
	}
	if (single_step) // single step then freeze AFTER this frame
	{
		freeze = true;
		single_step = false;
	}
	return true;
}

void connectMotionGraphToController(MotionGraphController* mgc, vector<vector<int> > TransitionPoints)
{
	// These hard-code file names need to come from somewhere external.
	string seqID1("swing1.bvh");
	string seqID2("swing2.bvh");
	int startFrame = 0;

	list<MotionGraphController::vertexTargets> path;
	MotionGraphController::vertexTargets temp;
	for (unsigned long i = 0; i < TransitionPoints.size(); i++)
	{
		temp.SeqID = seqID1;
		temp.SeqID2 = seqID2;
		temp.FrameNumber = TransitionPoints[i][0];// MS->numFrames();
		temp.FrameNumber2 = TransitionPoints[i][1];// 0;
		path.push_back(temp);
	}

	mgc->setPath(seqID1, startFrame, path);
}

void AnimationControl::loadCharacters(list<Object*>& render_list)
{
	logout << "Constructing MotionGraph" << endl;
	MotionGraph a(1);
	logout << "Constructing MotionGraphController" << endl;
	MotionGraphController *b = new MotionGraphController(a);
	logout << "Connecting MotionGraph to MotionGraphController" << endl;
	connectMotionGraphToController(b, a.transitionPoints);
	logout << "MotionGraph initialization complete" << endl;

	data_manager.addFileSearchPath(BVH_MOTION_FILE_PATH);
	char* BVH_filename = NULL;
	try
	{
		BVH_filename = data_manager.findFile(character_BVH.c_str());
		if (BVH_filename == NULL)
		{
			logout << "AnimationControl::loadCharacters: Unable to find character BVH file <" << character_BVH << ">. Aborting load." << endl;
			throw BasicException("ABORT");
		}
		pair<Skeleton*, MotionSequence*> read_result;
		try
		{
			read_result = data_manager.readBVH(BVH_filename);
		}
		catch (const DataManagementException& dme)
		{
			logout << "AnimationControl::loadCharacters: Unable to load character data files. Aborting load." << endl;
			logout << "   Failure due to " << dme.msg << endl;
			throw BasicException("ABORT");
		}

		Skeleton* skel = read_result.first;
		MotionSequence* ms = read_result.second;

		// scale the bones lengths and translation distances
		skel->scaleBoneLengths(character_size_scale);
		ms->scaleChannel(CHANNEL_ID(0,CT_TX), character_size_scale);
		ms->scaleChannel(CHANNEL_ID(0,CT_TY), character_size_scale);
		ms->scaleChannel(CHANNEL_ID(0,CT_TZ), character_size_scale);

		//MotionSequenceController* controller = new MotionSequenceController(ms);
		
		// create rendering model for the character and put the character's 
		// bone objects in the rendering list
		Color color(0.0f,0.4f,1.0f);
		skel->constructRenderObject(render_list, color);

		// attach motion controller to animated skeleton
		skel->attachMotionController(b);

		// create a character to link all the pieces together.
		string d1 = string("skeleton: ") + character_BVH;
		string d2 = string("motion: ") + character_BVH;
		skel->setDescription1(d1.c_str());
		skel->setDescription2(d2.c_str());
		character = skel;
	} 
	catch (BasicException&) { }

	strDelete(BVH_filename); BVH_filename = NULL;

	ready = true;
}

