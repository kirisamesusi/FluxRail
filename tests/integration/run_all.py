"""统一软件验证入口；编译/ARM模拟/网页测试，不上传固件。"""
from pathlib import Path
import argparse,subprocess,sys,json,hashlib
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip-build',action='store_true',help='只跑模拟/Web；已有正式构建由独立入口验证')
    args=parser.parse_args()
    logdir=HERE/'build/full';logdir.mkdir(parents=True,exist_ok=True)
    report={'steps':[],'passed':False,'status':'running','hardware_tested':False,'game_tested':False,'uploaded':False,'formal_build_included':not args.skip_build}
    target=HERE/'build/full_results.json'
    def publish():target.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    publish() # 开始即失效旧成功记录；失败也留下当前实际步骤，避免误用旧证据。
    def step(label,command):
        print('Running '+label,flush=True)
        logpath=logdir/(label.replace('/','_').replace(' ','_')+'.log')
        with logpath.open('w',encoding='utf-8') as log:
            result=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        report['steps'].append({'command':label,'exit_code':result.returncode,'log':logpath.relative_to(ROOT).as_posix()})
        if result.returncode:
            report['status']='failed';publish();raise SystemExit('Failed '+label+'; see '+str(logpath))
        publish()
    try:
        for script in ['tests/module1/run_tests.py','tests/module2/run_tests.py','tests/module3/run_tests.py',
                       'tests/integration/run_tests.py','tests/integration/run_transport.py',
                       'tests/integration/run_protocol.py','tests/integration/run_storage.py',
                       'tests/integration/run_app_storage.py','tests/integration/run_io4.py','tests/integration/run_motion.py']:
            step(script,[sys.executable,str(ROOT/script)])
        step('web_all',['node','--test','web/tests/web.test.mjs','web/tests/knob-preview.test.mjs','web/tests/integration.test.mjs','web/tests/integration-ui.test.mjs','web/tests/layout.test.mjs','web/tests/template-actions.test.mjs','web/tests/uf2.test.mjs'])
        if not args.skip_build:
            for script in ['tests/integration/compile_firmware.py','tests/integration/run_usb_descriptors.py']:
                step(script,[sys.executable,str(ROOT/script)])
    except Exception as error:
        report['status']='failed';report['error']=str(error);publish();raise
    report['passed']=True;report['status']='complete';publish()
    print('PASS: complete software validation; hardware/USB host/game not tested',flush=True)
if __name__=='__main__':main()
