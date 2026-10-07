package glass_ui.overlay;

import android.app.Activity;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputType;
import android.text.NoCopySpan;
import android.text.SpanWatcher;
import android.text.Spannable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

import java.nio.charset.StandardCharsets;

@SuppressWarnings("deprecation")
public final class ImeBridge {
    private static native Activity findActivity();
    private static native void onEdit(long owner, long token, long revision, long version,
            byte[] text, int start, int end, int composingStart, int composingEnd, boolean finished);
    private static native void onVisibleArea(long owner, float left, float top, float right, float bottom);

    private final long owner;
    private final Handler main = new Handler(Looper.getMainLooper());
    private Command pending;
    private volatile Command latest;
    private boolean posted;
    private volatile boolean disposed;
    private Activity activity;
    private ViewGroup root;
    private View previousFocus;
    private Editor editor;
    private InputMethodManager manager;
    private long token, revision, version, closedToken, keyboardRequest;
    private boolean applying, publishPosted, keyboardSeen;
    private Snapshot closedEdit;
    private Runnable hideCheck;
    private final ViewTreeObserver.OnGlobalLayoutListener layout = this::updateGeometry;

    public ImeBridge(long owner) { this.owner = owner; }

    public synchronized void update(long token, long revision, long platformVersion, long keyboardRequest,
            byte[] text, int start, int end, boolean password, float x, float y) {
        if (disposed) return;
        latest = pending = new Command(token, revision, platformVersion, keyboardRequest,
                text, start, end, password, x, y);
        if (!posted) {
            posted = main.post(this::drain);
        }
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
            Log.w("yuuki_glass_ui", "Android IME has no focused Activity");
        } catch (RuntimeException error) {
            Log.w("yuuki_glass_ui", "Android IME bridge failed", error);
        }
        closedToken = command.token;
        closedEdit = editor != null && token == command.token ? snapshot()
                : new Snapshot(command.text, command.start, command.end, -1, -1);
        version = Math.max(version, command.platformVersion) + 1;
        emit(command.token, command.revision, version, closedEdit, true);
        detach();
    }

    private boolean apply(Command command) {
        if (command.token == 0) {
            detach();
            return true;
        }
        if (command.token <= closedToken) {
            if (command.token == closedToken && closedEdit != null)
                emit(command.token, command.revision, ++version, closedEdit, true);
            return true;
        }
        Activity target = findActivity();
        if (target == null) return false;
        if (target != activity) {
            detach();
            attach(target);
        }
        boolean newSession = token != command.token;
        if (newSession) {
            token = command.token;
            revision = command.revision;
            version = 1;
            keyboardSeen = false;
            publishPosted = false;
            applying = true;
            try {
                editor.setInputType(InputType.TYPE_CLASS_TEXT | (command.password
                        ? InputType.TYPE_TEXT_VARIATION_PASSWORD : InputType.TYPE_TEXT_VARIATION_NORMAL));
                write(command);
                editor.requestFocus();
            } finally {
                applying = false;
            }
            emit(token, revision, version, snapshot(), false);
        } else if (revision != command.revision) {
            // Do not overwrite a newer IME edit with an old render snapshot.
            if (command.platformVersion == version || (command.platformVersion == 0 && version == 1)) {
                applying = true;
                try { write(command); } finally { applying = false; }
                ++version;
            }
            revision = command.revision;
            emit(token, revision, version, snapshot(), false);
        }
        editor.setTranslationX(command.x * Math.max(root.getWidth() - 1, 0));
        editor.setTranslationY(command.y * Math.max(root.getHeight() - 1, 0));
        if (newSession || keyboardRequest != command.keyboardRequest) {
            keyboardRequest = command.keyboardRequest;
            final long session = token, show = keyboardRequest;
            editor.post(() -> showKeyboard(session, show, 0));
        }
        updateGeometry();
        return true;
    }

    private void attach(Activity target) {
        activity = target;
        root = (ViewGroup) target.getWindow().getDecorView();
        previousFocus = target.getCurrentFocus();
        manager = (InputMethodManager) target.getSystemService(Activity.INPUT_METHOD_SERVICE);
        editor = new Editor(target);
        editor.setSingleLine(true);
        editor.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN);
        editor.setAlpha(0.0f);
        editor.setPadding(0, 0, 0, 0);
        editor.setFocusableInTouchMode(true);
        editor.setOnEditorActionListener((view, action, event) -> {
            if (action == EditorInfo.IME_ACTION_DONE || (event != null
                    && event.getKeyCode() == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_UP)) {
                finish();
                return true;
            }
            return false;
        });
        root.addView(editor, new FrameLayout.LayoutParams(1, 1));
        root.getViewTreeObserver().addOnGlobalLayoutListener(layout);
        Log.i("yuuki_glass_ui", "Android IME editor attached (Java)");
    }

    private void showKeyboard(long session, long show, int attempt) {
        if (disposed || token != session || keyboardRequest != show || editor == null) return;
        if (keyboardVisible()) return;
        boolean accepted = false;
        if (editor.isAttachedToWindow() && editor.hasWindowFocus()) {
            if (!editor.hasFocus()) editor.requestFocus();
            accepted = manager.showSoftInput(editor, 0);
        }
        // Accepted means a request was queued, not that a keyboard is visible.
        if ((Build.VERSION.SDK_INT >= 30 || !accepted) && attempt < 8)
            main.postDelayed(() -> showKeyboard(session, show, attempt + 1), 80);
        else if (!accepted)
            Log.w("yuuki_glass_ui", "IME show request failed: session=" + session
                    + " attached=" + editor.isAttachedToWindow() + " focus=" + editor.hasWindowFocus());
    }

    private boolean keyboardVisible() {
        if (Build.VERSION.SDK_INT < 30 || root == null) return false;
        WindowInsets insets = root.getRootWindowInsets();
        return insets != null && insets.isVisible(WindowInsets.Type.ime());
    }

    private void keyboardVisibility(boolean shown) {
        if (shown) {
            keyboardSeen = true;
            if (hideCheck != null) main.removeCallbacks(hideCheck);
            hideCheck = null;
        } else if (keyboardSeen && hideCheck == null) {
            final long session = token;
            hideCheck = () -> {
                hideCheck = null;
                if (token == session && editor != null && root.getRootWindowInsets() != null
                        && !keyboardVisible()) finish();
            };
            // Layout/insets can temporarily hide the IME while switching its mode.
            main.postDelayed(hideCheck, 400);
        }
    }

    private void write(Command command) {
        String text = new String(command.text, StandardCharsets.UTF_8);
        if (!text.contentEquals(editor.getText())) editor.setText(text);
        int start = utf16Offset(command.text, command.start);
        int end = utf16Offset(command.text, command.end);
        editor.setSelection(Math.min(start, text.length()), Math.min(end, text.length()));
    }

    private Snapshot snapshot() {
        Editable text = editor.getText();
        String value = text.toString();
        return new Snapshot(value.getBytes(StandardCharsets.UTF_8),
                byteOffset(value, editor.getSelectionStart()), byteOffset(value, editor.getSelectionEnd()),
                byteOffset(value, BaseInputConnection.getComposingSpanStart(text)),
                byteOffset(value, BaseInputConnection.getComposingSpanEnd(text)));
    }

    private void changed() {
        if (applying || disposed || token == 0 || publishPosted) return;
        ++version;
        publishPosted = true;
        final long session = token;
        main.post(() -> {
            if (token != session) return;
            publishPosted = false;
            if (!disposed && token == session && editor != null)
                emit(token, revision, version, snapshot(), false);
        });
    }

    private void emit(long session, long editRevision, long editVersion, Snapshot edit, boolean finished) {
        onEdit(owner, session, editRevision, editVersion, edit.text, edit.start, edit.end,
                edit.composingStart, edit.composingEnd, finished);
    }

    private void finish() {
        if (applying || token == 0 || editor == null) return;
        closedToken = token;
        closedEdit = snapshot();
        emit(token, revision, ++version, closedEdit, true);
        detach();
    }

    private void detach() {
        applying = true;
        if (hideCheck != null) main.removeCallbacks(hideCheck);
        hideCheck = null;
        token = 0;
        publishPosted = false;
        keyboardSeen = false;
        try {
            if (root != null && root.getViewTreeObserver().isAlive())
                root.getViewTreeObserver().removeOnGlobalLayoutListener(layout);
            if (editor != null) {
                boolean restore = editor.hasFocus();
                if (restore && manager != null)
                    manager.hideSoftInputFromWindow(editor.getWindowToken(), 0);
                editor.clearFocus();
                if (root != null) root.removeView(editor);
                if (restore && previousFocus != null) previousFocus.requestFocus();
            }
        } catch (RuntimeException error) {
            Log.w("yuuki_glass_ui", "Android IME cleanup failed", error);
        } finally {
            activity = null;
            root = null;
            editor = null;
            manager = null;
            previousFocus = null;
            applying = false;
            onVisibleArea(owner, 0, 0, 1, 1);
        }
    }

    private void updateGeometry() {
        if (root == null || root.getWidth() <= 0 || root.getHeight() <= 0) return;
        Rect visible = new Rect();
        int[] origin = new int[2];
        root.getWindowVisibleDisplayFrame(visible);
        root.getLocationOnScreen(origin);
        float left = clamp((visible.left - origin[0]) / (float) root.getWidth());
        float top = clamp((visible.top - origin[1]) / (float) root.getHeight());
        float right = clamp((visible.right - origin[0]) / (float) root.getWidth());
        float bottom = clamp((visible.bottom - origin[1]) / (float) root.getHeight());
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsets insets = root.getRootWindowInsets();
            if (insets != null) {
                Insets keyboard = insets.getInsets(WindowInsets.Type.ime());
                left = Math.max(left, keyboard.left / (float) root.getWidth());
                top = Math.max(top, keyboard.top / (float) root.getHeight());
                right = Math.min(right, 1 - keyboard.right / (float) root.getWidth());
                bottom = Math.min(bottom, 1 - keyboard.bottom / (float) root.getHeight());
                onVisibleArea(owner, clamp(left), clamp(top), clamp(right), clamp(bottom));
                boolean shown = insets.isVisible(WindowInsets.Type.ime());
                keyboardVisibility(shown);
                return;
            }
        }
        onVisibleArea(owner, left, top, right, bottom);
    }

    private static float clamp(float value) { return Math.max(0, Math.min(value, 1)); }
    private static int byteOffset(String text, int index) {
        return index < 0 ? -1 : text.substring(0, Math.min(index, text.length())).getBytes(StandardCharsets.UTF_8).length;
    }
    private static int utf16Offset(byte[] text, int bytes) {
        return new String(text, 0, Math.max(0, Math.min(bytes, text.length)), StandardCharsets.UTF_8).length();
    }

    private final class Editor extends EditText implements SpanWatcher, NoCopySpan {
        Editor(Activity context) {
            super(context);
            addTextChangedListener(new TextWatcher() {
                public void beforeTextChanged(CharSequence text, int start, int count, int after) {}
                public void onTextChanged(CharSequence text, int start, int before, int count) {}
                public void afterTextChanged(Editable text) { observe(text); changed(); }
            });
            observe(getText());
        }
        private void observe(Editable text) {
            text.setSpan(this, 0, text.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);
        }
        @Override protected void onSelectionChanged(int start, int end) {
            super.onSelectionChanged(start, end);
            changed();
        }
        @Override public void onWindowFocusChanged(boolean focused) {
            super.onWindowFocusChanged(focused);
            if (!focused) finish();
        }
        @Override public boolean onKeyPreIme(int key, KeyEvent event) {
            if (key == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
                finish();
                return true;
            }
            return super.onKeyPreIme(key, event);
        }
        public void onSpanAdded(Spannable text, Object what, int start, int end) { if (what != this) changed(); }
        public void onSpanRemoved(Spannable text, Object what, int start, int end) { if (what != this) changed(); }
        public void onSpanChanged(Spannable text, Object what, int oldStart, int oldEnd, int start, int end) {
            if (what != this) changed();
        }
    }

    private static final class Snapshot {
        final byte[] text;
        final int start, end, composingStart, composingEnd;
        Snapshot(byte[] text, int start, int end, int composingStart, int composingEnd) {
            this.text = text;
            this.start = start;
            this.end = end;
            this.composingStart = composingStart;
            this.composingEnd = composingEnd;
        }
    }

    private static final class Command {
        final long token, revision, platformVersion, keyboardRequest;
        final byte[] text;
        final int start, end;
        final boolean password;
        final float x, y;
        Command(long token, long revision, long platformVersion, long keyboardRequest, byte[] text, int start, int end,
                boolean password, float x, float y) {
            this.token = token;
            this.revision = revision;
            this.platformVersion = platformVersion;
            this.keyboardRequest = keyboardRequest;
            this.text = text;
            this.start = start;
            this.end = end;
            this.password = password;
            this.x = x;
            this.y = y;
        }
    }
}
