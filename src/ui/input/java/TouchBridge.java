package glass_ui.overlay;

import android.app.Activity;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowManager;

import java.util.ArrayList;

// Receives only app-window touches: Android dispatches floating IME keys to the IME window.
public final class TouchBridge {
    private static native Activity findActivity();
    private static native void onPointer(long owner, long generation, int phase, float x, float y, long timestampNs);
    private static native void onUnavailable(long owner, long generation);

    private final long owner;
    private final Handler main = new Handler(Looper.getMainLooper());
    private Command pending;
    private volatile Command latest;
    private boolean posted;
    private volatile boolean disposed;
    private Activity activity;
    private View root;
    private Windows windows;
    private long generation;
    private final ViewTreeObserver.OnGlobalLayoutListener layout = this::refresh;

    public TouchBridge(long owner) { this.owner = owner; }

    public synchronized void update(long generation, float[] regions) {
        if (disposed) return;
        latest = pending = new Command(generation, regions);
        if (!posted) posted = main.post(this::drain);
    }

    public synchronized void close() {
        if (disposed) return;
        disposed = true;
        latest = pending = null;
        main.post(this::detach);
    }

    private void drain() {
        Command command;
        synchronized (this) {
            command = pending;
            pending = null;
            posted = false;
        }
        if (command != null) dispatch(command, 0);
    }

    private void dispatch(Command command, int attempt) {
        if (disposed || latest != command) return;
        try {
            if (apply(command)) return;
            if (attempt < 8) {
                main.postDelayed(() -> dispatch(command, attempt + 1), 80);
                return;
            }
            Log.w("yuuki_glass_ui", "Android touch bridge has no focused Activity");
        } catch (RuntimeException error) {
            Log.w("yuuki_glass_ui", "Android touch bridge failed", error);
        }
        onUnavailable(owner, command.generation);
        detach();
    }

    private boolean apply(Command command) {
        if (command.generation == 0) {
            detach();
            return true;
        }
        Activity target = findActivity();
        if (target == null) return false;
        if (target != activity || generation != command.generation) {
            detach();
            activity = target;
            root = target.getWindow().getDecorView();
            windows = new Windows(target,
                    (phase, x, y, timestamp) -> onPointer(owner, generation, phase, x, y, timestamp));
            root.getViewTreeObserver().addOnGlobalLayoutListener(layout);
        }
        generation = command.generation;
        windows.setRegions(command.regions);
        return true;
    }

    private void refresh() {
        if (windows == null) return;
        try { windows.refresh(); }
        catch (RuntimeException error) {
            Log.w("yuuki_glass_ui", "Android touch geometry failed", error);
            onUnavailable(owner, generation);
            detach();
        }
    }

    private void detach() {
        generation = 0;
        try {
            if (root != null && root.getViewTreeObserver().isAlive())
                root.getViewTreeObserver().removeOnGlobalLayoutListener(layout);
            if (windows != null) windows.close();
        } catch (RuntimeException error) {
            Log.w("yuuki_glass_ui", "Android touch cleanup failed", error);
        } finally {
            activity = null;
            root = null;
            windows = null;
        }
    }

    private static final class Command {
        final long generation;
        final float[] regions;
        Command(long generation, float[] regions) {
            this.generation = generation;
            this.regions = regions;
        }
    }

    private interface Sink { void pointer(int phase, float x, float y, long timestampNs); }

    private static final class TouchView extends View {
        private final Sink sink;
        private float[] regions = new float[0];
        private int pointerId = -1;
        private float left, top, right = 1, bottom = 1;

        TouchView(Activity activity, Sink sink) {
            super(activity);
            this.sink = sink;
            setFocusable(false);
            setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        }

        void setRegions(float[] regions) { this.regions = regions; }

        void setWindowRegion(float left, float top, float right, float bottom, float shape) {
            this.left = left; this.top = top; this.right = right; this.bottom = bottom;
            setRegions(new float[]{0, 0, 1, 1, shape});
        }

        private boolean contains(float x, float y) {
            for (int index = 0; index + 4 < regions.length; index += 5) {
                float left = regions[index], top = regions[index + 1];
                float right = regions[index + 2], bottom = regions[index + 3];
                if (right <= left || bottom <= top || x < left || x > right || y < top || y > bottom)
                    continue;
                if (regions[index + 4] == 0) return true;
                float dx = (2 * x - left - right) / (right - left);
                float dy = (2 * y - top - bottom) / (bottom - top);
                if (dx * dx + dy * dy <= 1) return true;
            }
            return false;
        }

        @Override public boolean onTouchEvent(MotionEvent event) {
            if (getWidth() <= 0 || getHeight() <= 0) return false;
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_DOWN) {
                if (!contains(event.getX() / getWidth(), event.getY() / getHeight())) return false;
                pointerId = event.getPointerId(0);
                getParent().requestDisallowInterceptTouchEvent(true);
                send(event, 0, 0);
                return true;
            }
            int index = event.findPointerIndex(pointerId);
            if (action == MotionEvent.ACTION_CANCEL) {
                send(event, 3, Math.max(index, 0));
                pointerId = -1;
            } else if (index >= 0) {
                if (action == MotionEvent.ACTION_MOVE) send(event, 1, index);
                else if (action == MotionEvent.ACTION_UP || (action == MotionEvent.ACTION_POINTER_UP
                        && event.getPointerId(event.getActionIndex()) == pointerId)) {
                    send(event, 2, index);
                    pointerId = -1;
                    performClick();
                }
            }
            return true;
        }

        private void send(MotionEvent event, int phase, int index) {
            sink.pointer(phase, left + event.getX(index) / getWidth() * (right - left),
                    top + event.getY(index) / getHeight() * (bottom - top),
                    event.getEventTime() * 1_000_000L);
        }

        @Override public boolean performClick() { super.performClick(); return true; }
    }

    // Separate ViewRoots also receive touches in NativeActivity hosts whose native
    // input queue consumes events before the main window's ordinary View hierarchy.
    private static final class Windows {
        private final Activity activity;
        private final View root;
        private final WindowManager manager;
        private final Sink sink;
        private final ArrayList<TouchView> views = new ArrayList<>();
        private final ArrayList<WindowManager.LayoutParams> layouts = new ArrayList<>();
        private float[] regions = new float[0];

        Windows(Activity activity, Sink sink) {
            this.activity = activity;
            this.root = activity.getWindow().getDecorView();
            this.manager = activity.getWindowManager();
            this.sink = sink;
        }

        void setRegions(float[] regions) { this.regions = regions; refresh(); }

        void refresh() {
            if (!root.isAttachedToWindow() || root.getWidth() <= 0 || root.getHeight() <= 0) return;
            int count = regions.length / 5;
            while (views.size() > count) {
                int index = views.size() - 1;
                remove(views.remove(index));
                layouts.remove(index);
            }
            int[] origin = new int[2];
            root.getLocationOnScreen(origin);
            for (int index = 0; index < count; ++index) {
                int offset = index * 5;
                int x = origin[0] + Math.round(regions[offset] * root.getWidth());
                int y = origin[1] + Math.round(regions[offset + 1] * root.getHeight());
                int width = Math.max(1, Math.round((regions[offset + 2] - regions[offset]) * root.getWidth()));
                int height = Math.max(1, Math.round((regions[offset + 3] - regions[offset + 1]) * root.getHeight()));
                boolean adding = index == views.size();
                TouchView view;
                WindowManager.LayoutParams layout;
                if (adding) {
                    view = new TouchView(activity, sink);
                    layout = new WindowManager.LayoutParams(width, height,
                            WindowManager.LayoutParams.TYPE_APPLICATION_PANEL,
                            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                            | WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM
                            | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                            | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                            PixelFormat.TRANSLUCENT);
                    layout.gravity = Gravity.TOP | Gravity.LEFT;
                    layout.token = root.getWindowToken();
                    layout.softInputMode = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING;
                    layout.setTitle("yuuki_glass_ui-input");
                } else {
                    view = views.get(index); layout = layouts.get(index);
                }
                view.setWindowRegion(regions[offset], regions[offset + 1], regions[offset + 2],
                        regions[offset + 3], regions[offset + 4]);
                boolean moved = layout.x != x || layout.y != y || layout.width != width || layout.height != height;
                layout.x = x; layout.y = y; layout.width = width; layout.height = height;
                if (adding) {
                    manager.addView(view, layout);
                    views.add(view); layouts.add(layout);
                } else if (moved) manager.updateViewLayout(view, layout);
            }
        }

        void close() {
            for (TouchView view : views) remove(view);
            views.clear(); layouts.clear();
        }

        private void remove(TouchView view) {
            try { manager.removeViewImmediate(view); }
            catch (IllegalArgumentException ignored) {
                // Activity teardown can remove attached windows before our cleanup.
            }
        }
    }
}
