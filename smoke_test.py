"""Diagnóstico manual de modelos e interface; importar não inicia recursos."""


def main() -> None:
    from pathlib import Path
    import numpy as np
    from teleport_cam.vision import Detector,Segmenter
    from teleport_cam.app import App
    from teleport_cam.devices import list_cameras
    from teleport_cam.effect_catalog import LABELS
    import tkinter as tk

    root=Path(__file__).resolve().parent
    from setup_models import verify_model
    from teleport_cam.model_assets import MODEL_ASSETS
    for name, (_, expected) in MODEL_ASSETS.items():
        verify_model(root / 'models' / name, expected)
    blank=np.zeros((720,1280,3),np.uint8)
    detector=Detector(root/'models')
    try:
        assert detector.detect(blank,1.0)==(False,0)
    finally:
        detector.close()
    segmenter=Segmenter(root/'models')
    try:
        result=segmenter.capture(blank,1.0)
        assert result.mask.shape==(720,1280)
        assert np.isfinite(result.mask).all()
        print('SEGMENTER_OK: mask shape/range',result.mask.shape,float(result.mask.min()),float(result.mask.max()))
    finally:
        segmenter.close()
    print('MODELS_OK: hand, face and silhouette initialized and executed')
    print('CAMERAS:',[(c.index,c.name) for c in list_cameras()])
    window=tk.Tk()
    window.withdraw()
    app=App(window)
    try:
        window.update_idletasks()
        assert app.engine is None
        assert not app.leave_enabled.get()
        assert app.countdown.deadline is None
        assert len(app.inputs)==4
        assert app.effect.get() in LABELS
        app.toggle_settings()
        assert app.settings_visible
        app.hide_settings()
        assert not app.settings_visible
        app.hotkey.ready.wait(1)
        print('HOTKEY_OK: global F8 registered =',app.hotkey.registered)
    finally:
        app.close()
    print('GUI_OK: controls initialize without opening webcam')


if __name__ == "__main__":
    main()
