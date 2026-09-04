#pragma once
#include "modules/Module.h"

class CoreCommands : public Module
{
public:
    CoreCommands();
    ~CoreCommands() override = default;

    void onLoad() override;
    void onEnable() override;
    void onDisable() override;
    void onUnload() override;
};
