#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class ActBeforePerformingLayoutPlugin : public eld::plugin::LinkerPlugin {
public:
  ActBeforePerformingLayoutPlugin()
      : eld::plugin::LinkerPlugin("ActBeforePerformingLayoutPlugin") {}
  void ActBeforePerformingLayout() override {
    if (!getLinker()->isLinkStateActBeforePerformingLayout()) {
      getLinker()->reportDiag(
          getLinker()->getErrorDiagID("Incorrect link state"));
      return;
    }
    getLinker()->reportDiag(
        getLinker()->getNoteDiagID("In ActBeforePerformingLayout"));
  }
};

ELD_REGISTER_PLUGIN(ActBeforePerformingLayoutPlugin)
