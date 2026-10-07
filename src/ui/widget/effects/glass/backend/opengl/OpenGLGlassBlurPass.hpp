#pragma once

#include "OpenGLGlassResources.hpp"
#include "ui/widget/effects/glass/GlassBlurPolicy.hpp"

namespace glass_ui::widget::opengl_glass {

bool renderGlassBlur(Resources &resources, GLuint source,
                     const GlassBlurPlan &plan);

}
