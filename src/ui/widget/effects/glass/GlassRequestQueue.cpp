#include "GlassRequestQueue.hpp"

#include "ui/widget/foundation/drawing/DrawTransform.hpp"

#include <algorithm>

namespace glass_ui::widget {

void GlassRequestQueue::beginFrame(GlassBackdropStyle backdropStyle,
                                   float density) {
  requests_.clear();
  backdropStyle_ = backdropStyle;
  density_ = density;
  sealed_ = false;
}

void GlassRequestQueue::submit(GlassRequest request) {
  if (sealed_)
    return;
  requests_.push_back(request);
}

void GlassRequestQueue::applyTransforms(const DrawTransformStore &transforms) {
  if (sealed_)
    return;
  for (GlassRequest &request : requests_) {
    const DrawTransform *transform = transforms.find(request.drawList);
    if (!transform)
      continue;
    request.min = transform->mapPoint(request.min);
    request.max = transform->mapPoint(request.max);
    request.clipRect = transform->mapClipRect(request.clipRect);
    const float radiusScale = std::min(transform->scale.x, transform->scale.y);
    request.radii.topLeft *= radiusScale;
    request.radii.topRight *= radiusScale;
    request.radii.bottomRight *= radiusScale;
    request.radii.bottomLeft *= radiusScale;
  }
}

void GlassRequestQueue::seal() {
  if (sealed_)
    return;
  sealed_ = true;
}

}
