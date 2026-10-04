const {app,BrowserWindow}=require('electron');const path=require('node:path');const assert=require('node:assert/strict');
const filename=process.argv[2];assert(['cold_io4_focus.html','recovery_hold.html'].includes(filename));let win;
app.whenReady().then(()=>{win=new BrowserWindow({width:1000,height:650,show:true,webPreferences:{sandbox:true,contextIsolation:true,nodeIntegration:false}});win.webContents.setWindowOpenHandler(()=>({action:'deny'}));win.loadFile(path.join(__dirname,filename));win.once('ready-to-show',()=>{win.show();win.focus();});});
app.on('window-all-closed',()=>app.quit());
