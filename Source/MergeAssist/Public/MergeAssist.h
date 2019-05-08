#pragma once

#include "Modules/ModuleManager.h"

class UBlueprint;

class IMergeAssistModule : public IModuleInterface
{
public:
	virtual void GenerateMergeAssistWidget(UBlueprint* BaseBlueprint, UBlueprint* LocalBlueprint, UBlueprint* RemoteBlueprint, UBlueprint* TargetBlueprint) = 0;
};
