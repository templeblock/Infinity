#define GLEW_STATIC
#include <windows.h>
#include <iniparser.h>
#include <cstdlib>
#include <gl\glee.h>
#include "UI.h"
#include "Framework.h"
#include <SOIL.h> // remove later

namespace UI
{
	Stage::Stage():	mFocused(0), mDoAllocate(true),
		mLastVisited(0), mPhase(PHASE_DEFAULT)
	{
	}

	Stage& Stage::queueRelayout()
	{
		if (mPhase != PHASE_ALLOCATE) {mDoAllocate = true;}	return *this;
	}
	
	Stage& Stage::captureFocus(Actor* focused)
	{
		if (mFocused != focused)
		{
			if (mFocused) mFocused->onFocusOut();
			mFocused = focused;
			if (mFocused) mFocused->onFocusIn();
		}
		return *this;
	}

	Stage& Stage::releaseFocus()
	{
		if (mFocused) mFocused->onFocusOut();
		mFocused = 0; return *this;
	}

	void Stage::onKeyDown(const KeyEvent& event)
	{
		//Also here should be and hotkeys handling

		if (mFocused)
			mFocused->onKeyDown(event);

		//No do not sent any event to childs
	}

	void Stage::onKeyUp(const KeyEvent& event)
	{
		if (mFocused)
			mFocused->onKeyUp(event);

		//No do not sent any event to childs
	}

	void Stage::onTouch(const ButtonEvent& event)
	{
		Actor* actor = doPick((u32)event.x, (u32)event.y);

		if (actor)
			actor->onTouch(event);
	}

	void Stage::onUntouch(const ButtonEvent& event)
	{
		Actor* actor = doPick((u32)event.x, (u32)event.y);

		if (actor)
			actor->onUntouch(event);
	}

	void Stage::onMotion(const MotionEvent& event)
	{
		Actor* actor = doPick((u32)event.x, (u32)event.y);

		if (mLastVisited != actor)
		{
			if (mLastVisited)
				mLastVisited->onLeave();

			if (actor)
			{
				actor->onEnter();
				//for (int button = 0; button < MouseButtons::Count; ++button)
				//{
				//	int	curState = mPrevButtonState[button];

				//	ButtonEvent evtTouch = {button, mModifiers, x, y};

				//	if (curState == InputState::Pressed)
				//		actor->onTouch(evtTouch);
				//}
			}
		}

		if (actor)
			actor->onMotion(event);

		mLastVisited = actor;
	}

	static const u32 rMaskUsed = 8;
	static const u32 gMaskUsed = 8;
	static const u32 bMaskUsed = 8;

	static const u32 rMask = 7;
	static const u32 gMask = 7;
	static const u32 bMask = 7;

	//code from clutter!!!!!!!!!!!!!!!!!!!!LGPL????????????????????????
	Color idToColor(u32 id)
	{
		Color color;
		u32 red, green, blue;

		// compute the numbers we'll store in the components
		red   = (id >> (gMaskUsed+bMaskUsed)) & (0xff >> (8-rMaskUsed));
		green = (id >> bMaskUsed) & (0xff >> (8-gMaskUsed));
		blue  = (id) & (0xff >> (8-bMaskUsed));

		// shift left bits a bit and add one, this circumvents
		// at least some potential rounding errors in GL/GLES
		// driver / hw implementation.
		if (rMaskUsed != rMask)
		red = red * 2 + 1;
		if (gMaskUsed != gMask)
		green = green * 2 + 1;
		if (bMaskUsed != bMask)
		blue  = blue  * 2 + 1;

		// shift up to be full 8bit values
		red   = red   << (8 - rMask);
		green = green << (8 - gMask);
		blue  = blue  << (8 - bMask);

		color.red   = (u8)red;
		color.green = (u8)green;
		color.blue  = (u8)blue;
		color.alpha = 0xff;

		return color;
	}

	u32 pixelToId (u8 pixel[4])                 
	{
		u8  red, green, blue;

		// reduce the pixel components to the number of bits actually used of the 8bits.
		red   = pixel[0] >> (8 - rMask);
		green = pixel[1] >> (8 - gMask);
		blue  = pixel[2] >> (8 - bMask);

		// divide potentially by two if 'fuzzy'
		red   = red   >> (rMaskUsed - rMask);
		green = green >> (gMaskUsed - gMask);
		blue  = blue  >> (bMaskUsed - bMask);  

		// combine the correct per component values into the final id
		u32 id =  blue + (green << bMaskUsed) + (red << (bMaskUsed + gMaskUsed));

		return id;
	} 
	
	void Stage::outlineActors()
	{
		mVG.begin();

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClearDepth(1.0f);
		glClearStencil(0);
		glDepthMask(GL_TRUE);
		glStencilMask( 0xFF );
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glUseProgram(0);
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);

		for(u32 i = 0; i < mRenderQueue.size(); ++i)
		{
			glPushMatrix();
			glMultMatrixf(mRenderQueue[i].transform);
			mRenderQueue[i].actor->onPick(idToColor(i));
			glPopMatrix();
		}

		mVG.end();
		glFinish();
	}

	Actor* Stage::doPick(u32 x, u32 y)
	{
		GLint	viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		
		// Read the color of the screen co-ords pixel
		u8	pixel[4];
		glReadPixels (x, viewport[3]-y-1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		
		//u8* p = new u8[mWidth*mHeight*4];
		//glReadPixels (0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, p);
		//SOIL_save_image("pick.bmp", SOIL_SAVE_TYPE_BMP, mWidth, mHeight, 4, p);		
		//delete [] p;

		if (pixel[0] == 0xff && pixel[1] == 0xff && pixel[2] == 0xff)
			return 0/*this*/;

		u32 id = pixelToId(pixel);
		assert(id < mRenderQueue.size());
		return mRenderQueue[id].actor;
	}

	void Stage::enterPhase(Phase nextPhase)
	{
		switch (nextPhase)
		{
			case PHASE_EVENT_HANDLING:
				outlineActors();
				break;

			case PHASE_ALLOCATE:
				if (mDoAllocate)
				{
					mDoAllocate = false;
					onAllocate();

					mRenderQueue.clear();
					updateRenderQueue(this, getTransformMatrix());
				}
				break;

			case PHASE_RENDERING:
				renderActors();
				glFlush();
				break;

			case PHASE_DEFAULT:
				break;

			default:
				assert(0);
		}

		mPhase = nextPhase;
	}

	void Stage::renderActors()
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		//glClearDepth(1.0);
		glClear(GL_COLOR_BUFFER_BIT/* | GL_DEPTH_BUFFER_BIT*/);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
	
		mVG.begin();
	
		onPaint();

		std::vector<RenderItem>::iterator	it  = mRenderQueue.begin(),
											end = mRenderQueue.end();

		for (; it != end; ++it)
		{
			glPushMatrix();
			glMultMatrixf((*it).transform);
			(*it).actor->onPaint();
			glPopMatrix();
		}

		mVG.end();

		glFlush();
	}

	void Stage::updateRenderQueue(Container* container, const glm::mat4& parentTransform)
	{
		ChildsVector::iterator	it  = container->mChilds.begin(),
								end = container->mChilds.end();
		for (; it != end; ++it)
		{
			Actor* child = *it;

			if (child->isVisible())
			{
				glm::mat4	actorTransform = parentTransform*child->getTransformMatrix();
				RenderItem	item = {actorTransform, child};

				mRenderQueue.push_back(item);

				Container* childAsContainer = dynamic_cast<Container*>(child);
				if (childAsContainer)
					updateRenderQueue(childAsContainer, actorTransform);
			}
		}
	}

}