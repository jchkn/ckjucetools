/*
  ==============================================================================

    AudioProcessorGraphMultiThreaded.h
    Created: 26 Nov 2012 11:35:49am
    Author:  Christian

  ==============================================================================
*/

#ifndef __SCALECONTAINER_H__
#define __SCALECONTAINER_H__

#include "JuceHeader.h"


class ScaleableContainer : public Component
{
public : 
	ScaleableContainer(Component * componentToScale_, float initialScaleFactor, bool ownsComponent_) :
	   componentToScale(componentToScale_),ownsComponent(ownsComponent_),scaleFactor(-1.f)
	   {
		   if (componentToScale!=nullptr)
		   {
			   setSize(componentToScale->getWidth(), componentToScale->getHeight());			
			   addAndMakeVisible(componentToScale);
		   };
		   setScaleFactor(initialScaleFactor);
	   };

	   ~ScaleableContainer()
	   {
		   if (ownsComponent) delete componentToScale;
	   };

	   void setScaleFactor(float newScaleFactor)
	   {
		   if (scaleFactor!=newScaleFactor)
		   {
			   scaleFactor=newScaleFactor;
			   if (componentToScale!=nullptr)
			   {
				   componentToScale->setTransform(AffineTransform::scale((float)scaleFactor));
				   resized();
			   };
		   };
	   };

	   void resized()
	   {
		   if (componentToScale!=nullptr && scaleFactor != 0.f)
		   {
			   componentToScale->setBounds(0,0,(int)(getWidth()/scaleFactor),(int)(getHeight()/scaleFactor));
		   }
	   }

private:
	Component* componentToScale;
	bool ownsComponent;
	float scaleFactor;
};





#endif  // __SCALECONTAINER_H__
