"""Stage a launchable macOS app around the native client and exact cooked assets."""
import argparse,plistlib,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--executable',type=Path,required=True);p.add_argument('--assets',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--bundle-id',default='com.xanderjm.humanresources');p.add_argument('--name',default='Human Resources');a=p.parse_args()
macos=a.output/'Contents/MacOS';resources=a.output/'Contents/Resources';macos.mkdir(parents=True,exist_ok=True);resources.mkdir(parents=True,exist_ok=True)
shutil.copy2(a.executable,macos/'human_resources_player')
if (resources/'assets').exists():shutil.rmtree(resources/'assets')
shutil.copytree(a.assets,resources/'assets')
with (a.output/'Contents/Info.plist').open('wb') as f:plistlib.dump({'CFBundleIdentifier':a.bundle_id,'CFBundleName':a.name,'CFBundleDisplayName':a.name,'CFBundleExecutable':'human_resources_player','CFBundlePackageType':'APPL','CFBundleShortVersionString':'0.1.0','CFBundleVersion':'1','LSMinimumSystemVersion':'13.0','NSHighResolutionCapable':True},f)

subprocess.run(["/usr/bin/codesign", "--force", "--deep", "--sign", "-", str(a.output)],check=True)
