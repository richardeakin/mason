
#pragma once

#include "mason/Common.h"
#include "ui/Suite.h"
#include "mason/FileWatcher.h"
#include "ui/Label.h"
#include "mason/ParticleSystemGpu.h"
//#include "mason/DartBag.h"
#include "mason/Var.h"

#include "cinder/CameraUi.h"
#include "cinder/AxisAlignedBox.h"

class BlendingTest : public ui::SuiteView {
  public:
	BlendingTest();

	void layout() override;
	void update() override;
	void draw( ui::Renderer *ren )	override;

  private:
	bool touchesBegan( ci::app::TouchEvent &event ) override;

	bool	keyDown( ci::app::KeyEvent &event ) override;
	void	setupParams();
	void	setupScene();
	void	setupTF();
	void	setupGlsl();
	void	setupUI();
	void	resetCam();
	void	centerCam();

	void	updateUI();
	void	renderParticles();

	ma::Var<ci::vec3>		mWorldDims;
	ma::Var<int>			mNumParticlesA;
	ma::Var<ci::ColorA>		mBackgroundColor;

	struct ParticleGroup {
		enum Type { PARTICLES, RECT, SPHERE };

		ci::vec3	pos;
		ci::ColorA	color;
	bool		depthTest;
		bool		depthWrite;
		bool		enableBlend = true;
		std::string	blendMode;
		std::string	depthFunc;
		Type		type;
	};

	std::vector<ParticleGroup>	mGroups;


	// TODO: need to work on ScopedWatch container of some sort, this is outa hand
	mason::ScopedWatch	mScriptWatch, mWatchGlslUpdate, mWatchGlslRender, mWatchGlslDepthTexture;

	ui::LabelGridRef	mInfoLabel;

	ci::CameraUi			mCamUi;
	ci::CameraPersp			mCam, mCamInitial;
	ci::gl::BatchRef		mBatchGrid, mBatchBox;
	ci::gl::GlslProgRef		mGlslDepthTexture;
	ci::AxisAlignedBox		mWorldBounds;

	ci::gl::BatchRef			mBatchSphere;
	ma::ParticleSystemGpuRef	mParticleSystem;
	ci::gl::FboRef				mFbo;

	double mSimulationTime = 0; // need to store this to update TF on reload

	bool mDrawFrame = true;
	bool mPauseSim = false;
	bool mDrawSpheres = true;
	bool mDrawParticleSystem = true;
	bool mDrawParticlesToFbo = true;
	bool mDrawWireframe = true;
};
