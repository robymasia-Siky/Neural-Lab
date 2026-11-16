import os, glob, zipfile, urllib.request, shutil

ABLETON_OSC_ZIP = 'https://github.com/ideoforms/AbletonOSC/archive/refs/heads/master.zip'

def find_ableton_remote_scripts_dirs():
    candidates = [
        os.path.expandvars(r'%ProgramData%\Ableton\Live *\Resources\MIDI Remote Scripts'),
        os.path.expandvars(r'%LOCALAPPDATA%\Programs\Ableton\Live *\Resources\MIDI Remote Scripts'),
        os.path.expanduser(r'~\Documents\Ableton\User Library\Remote Scripts')
    ]
    found = []
    for pattern in candidates:
        for path in glob.glob(pattern):
            if os.path.isdir(path):
                found.append(path)
    if not found:
        user_rs = os.path.expanduser(r'~\Documents\Ableton\User Library\Remote Scripts')
        os.makedirs(user_rs, exist_ok=True)
        found.append(user_rs)
    return found

def ensure_ableton_osc_installed():
    target_dirs = find_ableton_remote_scripts_dirs()
    installed_any = False
    needs_install = [d for d in target_dirs if not os.path.exists(os.path.join(d, 'AbletonOSC', '__init__.py'))]
    if not needs_install:
        return True
    tmp_zip = os.path.join(os.environ.get('TEMP', '.'), 'AbletonOSC.zip')
    tmp_ext = os.path.join(os.environ.get('TEMP', '.'), 'AbletonOSC_ext')
    try:
        print('[AbletonOSC] Scaricamento automatico Remote Script...')
        urllib.request.urlretrieve(ABLETON_OSC_ZIP, tmp_zip)
        if os.path.exists(tmp_ext): shutil.rmtree(tmp_ext)
        with zipfile.ZipFile(tmp_zip, 'r') as zf:
            zf.extractall(tmp_ext)
        src_folder = os.path.join(tmp_ext, 'AbletonOSC-master')
        for dest_root in needs_install:
            dest_folder = os.path.join(dest_root, 'AbletonOSC')
            if os.path.exists(dest_folder): shutil.rmtree(dest_folder)
            shutil.copytree(src_folder, dest_folder)
            print(f'[AbletonOSC] Installato con successo in: {dest_folder}')
            installed_any = True
    except Exception as e:
        print(f'[AbletonOSC Warning] Installazione automatica non riuscita: {e}')
    finally:
        if os.path.exists(tmp_zip): os.remove(tmp_zip)
        if os.path.exists(tmp_ext): shutil.rmtree(tmp_ext)
    return installed_any

if __name__ == '__main__':
    ensure_ableton_osc_installed()
