"""Chromium offline checks of the delivered HTML, downloads and render controls."""
from pathlib import Path
from urllib.parse import unquote, urlparse
import json
import os

from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parents[1]


def run():
    result=[]
    with sync_playwright() as p:
        path=os.environ.get('FLUXRAIL_CHROMIUM',r'C:\Program Files\Google\Chrome\Application\chrome.exe')
        browser=p.chromium.launch(executable_path=path,headless=True,
                                  args=['--use-angle=swiftshader','--enable-unsafe-swiftshader','--disable-gpu-sandbox'])
        page=browser.new_page(viewport={'width':1400,'height':820},accept_downloads=True)
        errors=[]
        network=[]
        page.on('pageerror',lambda error:errors.append(str(error)))
        page.on('request',lambda r:network.append(r.url) if r.url.startswith(('http:','https:')) else None)
        for machine,file in [(False,'旋钮模型.html'),(True,'整机模型.html')]:
            page.goto((ROOT/file).as_uri(),wait_until='load')
            page.wait_for_function("document.body.dataset.ready === 'yes'")
            total=int(page.locator('body').get_attribute('data-visible'))
            assert total==(268 if machine else 63)
            stage=page.locator('#stage')
            assert stage.bounding_box()['height']<900
            if not machine:
                page.locator('#hide').check()
                assert int(page.locator('body').get_attribute('data-visible'))==61
                page.locator('#hide').uncheck()
                page.locator('#angle').evaluate("(el) => {el.value=91; el.dispatchEvent(new Event('input'));}")
                assert page.locator('#angle-label').inner_text()=='91°'
            else:
                page.locator('#variant').select_option('音击')
                assert int(page.locator('body').get_attribute('data-visible'))==268
            page.locator('#amount').evaluate("(el)=>{el.value=86; el.dispatchEvent(new Event('input')); el.dispatchEvent(new Event('change'));}")
            assert page.locator('#amount-label').inner_text()=='86%'
            page.screenshot(path=ROOT/'验证'/('整机预览.png' if machine else '旋钮爆炸预览.png'))
            page.locator('#pick').select_option('bearing-686')
            page.locator('#isolate').check()
            assert int(page.locator('body').get_attribute('data-visible'))==(6 if machine else 3)
            link=page.get_by_role('link',name='STEP',exact=True)
            assert unquote(link.get_attribute('href')).endswith('.step')
            with page.expect_download() as info:
                link.click()
            relative=unquote(link.get_attribute('href'))
            assert info.value.path().read_bytes()==(ROOT/relative).read_bytes()
            stl=page.get_by_role('link',name='参考 STL',exact=True)
            with page.expect_download() as mesh:
                stl.click()
            assert mesh.value.path().read_bytes()==(ROOT/unquote(stl.get_attribute('href'))).read_bytes()
            page.locator('#reset').click()
            assert page.locator('#amount-label').inner_text()=='0%'
            assert int(page.locator('body').get_attribute('data-visible'))==total
            if not machine:
                page.screenshot(path=ROOT/'验证/旋钮总成预览.png')
            result.append(dict(file=file,initialCADInstances=total,webgl=True,explode=True,
                               isolateAndDownload=True,reset=True,variantSwitch=machine))
        for file in ('index.html','采购说明.html','打印装配.html','单件下载.html'):
            page.goto((ROOT/file).as_uri())
            missing=[]
            for link in page.locator('a').evaluate_all("(links)=>links.map(a=>a.href)"):
                url=urlparse(link)
                if url.scheme!='file': continue
                path=Path(unquote(url.path).lstrip('/'))
                if not path.is_file(): missing.append(link)
            assert not missing,(file,missing)
        for file in ('旋钮/加工图/r2尺寸图.svg',):
            page.goto((ROOT/file).as_uri())
            assert page.locator('svg').count()==1
            page.locator('svg').screenshot(path=ROOT/'验证'/(Path(file).stem+'.png'))
        page.set_viewport_size({'width':450,'height':900})
        page.goto((ROOT/'旋钮模型.html').as_uri())
        page.wait_for_function("document.body.dataset.ready === 'yes'")
        assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
        assert not errors,errors
        assert not network,network
        browser.close()
    (ROOT/'验证/离线网页.json').write_text(json.dumps(dict(pages=result,errors=errors,network=network,
                                                       smallViewport=True,allLocalLinks=True),ensure_ascii=False,indent=2)+'\n',
                                         encoding='utf-8')


if __name__=='__main__':
    run()
