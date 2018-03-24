/* Created By Yang */
/*
||
|| @file 	FBD.h
|| @version	2.0
|| @author	Yang 
|| @contact	2435575291@qq.com
||
|| @description
|| Definitions of Function Blocks
||
*/

#include "FBD.h"

TON::TON(uint32_t defaultPT)
{
	ET = millis();
	PT = defaultPT;
	IN = false;
	PRE = false;
	Q = false;
}

// When any condition is true, it returns true
void TON::update()
{
	if (IN != PRE)
	{
		PRE = IN;
		if (IN == true)
			ET = millis();
	}

	if (IN)
	{
		if ((ET + PT) <= millis())
			Q = true;
	}
	else
	{
		ET = millis();
		Q = false;
	}
}

void TON::reset()
{
	ET = millis();
	IN = false;
	PRE = false;
	Q = false;
}

TOF::TOF(uint32_t defaultPT)
{
	ET = millis();
	PT = defaultPT;
	IN = false;
	PRE = false;
	Q = false;
}

void TOF::reset()
{
	ET = millis();
	IN = false;
	PRE = false;
	Q = false;
}

// When any condition is true, it returns true
void TOF::update()
{
	if (IN != PRE)
	{
		PRE = IN;
		if (IN == false)
			ET = millis();
	}

	if (IN == false)
	{
		if ((ET + PT) <= millis())
			Q = false;
		else
			Q = true;
	}
	else
	{
		ET = millis();
		Q = true;
	}
}

TP::TP(uint32_t defaultPT)
{
	EN = false;
	IN = false;
	PRE = false;
	Q = false;
	PT = defaultPT; 
	ET = millis();
}

void TP::reset()
{
	ET = millis();
	IN = false;
	PRE = false;
	Q = false;
}

// When any condition is true, it returns true
void TP::update()
{
	if (IN != PRE)
	{
		PRE = IN;
		if (IN == true)
			ET = PT + millis();
	}

	if (ET > millis())
		Q = true;
	else
	{
		Q = false;
		ET = millis();
	}
}

Rtrg::Rtrg()
{
	IN = false;
	PRE = false;
	Q = false;
}

void Rtrg::reset()
{
	IN = false;
	PRE = false;
	Q = false;
}

void Rtrg::update()
{
	Q = false;
	if(IN != PRE)
	{
		PRE = IN;
		if (PRE == true)
			Q = true;
	}
}

Ftrg::Ftrg()
{
	IN = false;
	PRE = false;
	Q = false;
}

void Ftrg::reset()
{
	IN = false;
	PRE = false;
	Q = false;
}

void Ftrg::update()
{
	Q = false;
	if (IN != PRE)
	{
		PRE = IN;
		if (IN == false)
			Q = true;
	}
}
