const {app,BrowserWindow}=require('electron');let win;
app.whenReady().then(()=>{win=new BrowserWindow({width:1000,height:650,show:true,webPreferences:{sandbox:true,contextIsolation:true,nodeIntegration:false}});win.loadFile(require('node:path').join(__dirname,'recovery_hold.html'));win.once('ready-to-show',()=>{win.show();win.focus();});});
app.on('window-all-closed',()=>app.quit());
