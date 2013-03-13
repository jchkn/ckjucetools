
#ifndef __MultiProcessPropertyFileWrapper_H__
#define __MultiProcessPropertyFileWrapper_H__

#include "JuceHeader.h"

// Not enough tested in the moment!

// Drop-in Replacement for PropertiesFile which is usable in MultiProcess environments
// Of cause, this has no database-behavior, but prevents situations like
// App A sets parameter A
// App B sets parameter B -> overwrite settings in A

class MultiProcessPropertiesFileWrapper
{

public: 
	
	MultiProcessPropertiesFileWrapper(PropertiesFile * propertiesFileNonOwned)
		: propertiesFile(propertiesFileNonOwned)
	{
		
	};

	virtual ~MultiProcessPropertiesFileWrapper()
	{

	};


	String getValue (const String& keyName,  const String& defaultReturnValue = String::empty)   
	{
		checkForModifications() ;
		return propertiesFile->getValue(keyName, defaultReturnValue);
	}


	int getIntValue (const String& keyName,
		const int defaultReturnValue = 0)  
	{
		checkForModifications() ;
		return propertiesFile->getIntValue(keyName,defaultReturnValue);
	}


	double getDoubleValue (const String& keyName,
		const double defaultReturnValue = 0.0)  
	{
		checkForModifications() ;
		return propertiesFile->getDoubleValue(keyName,defaultReturnValue);
	}


	bool getBoolValue (const String& keyName,
		const bool defaultReturnValue = false)  
	{
		checkForModifications() ;
		return propertiesFile->getBoolValue(keyName,defaultReturnValue);
	}


	XmlElement* getXmlValue (const String& keyName) 
	{
		checkForModifications() ;
		return propertiesFile->getXmlValue(keyName);
	}


	void setValue (const String& keyName, const var& value)
	{
		checkForModifications() ;
		propertiesFile->setValue(keyName,value);
		propertiesFile->saveIfNeeded();
		updateModificationTime();
	}


	void setValue (const String& keyName, const XmlElement* xml)
	{
		checkForModifications() ;
		propertiesFile->setValue(keyName,xml);
		propertiesFile->saveIfNeeded();
		updateModificationTime();
	}

	PropertiesFile* getOriginalPropertiesFile()
	{
		return propertiesFile;
	}


private:
	PropertiesFile * propertiesFile;

	void checkForModifications() 
	{
		if (lastModificationTime!=propertiesFile->getFile().getLastModificationTime())
		{
			propertiesFile->reload();			
			updateModificationTime();
		}
	};

	void updateModificationTime()
	{
		lastModificationTime=propertiesFile->getFile().getLastModificationTime();
	}

	Time lastModificationTime;
};


#endif

