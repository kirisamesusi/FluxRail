param([string]$Port = 'COM29')
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$p=[System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$p.ReadTimeout=5000; $p.WriteTimeout=5000; $p.NewLine="`n"; $p.DtrEnable=$true; $p.RtsEnable=$false
try {
 $p.Open(); [Console]::WriteLine('{"bridge":"open"}')
 while($null -ne ($line=[Console]::ReadLine())) {
  if($line -eq '__exit__'){break}
  try {
   if($line.StartsWith('__ctl__')) {
    $c=$line.Substring(7)|ConvertFrom-Json
    if($c.action -eq 'reopen') {
     $p.Close(); Start-Sleep -Milliseconds 150; $p.Open(); [Console]::WriteLine('{"bridge":"reopened"}')
    } elseif($c.action -eq 'dtr') {
     $p.DtrEnable=$false; Start-Sleep -Milliseconds 150; $p.DtrEnable=$true; Start-Sleep -Milliseconds 100; [Console]::WriteLine('{"bridge":"dtr"}')
    } elseif($c.action -eq 'signals') {
     $p.DtrEnable=[bool]$c.dtr; [Console]::WriteLine('{"bridge":"signals"}')
    } elseif($c.action -eq 'exchange') {
     $bytes=[Convert]::FromBase64String($c.data)
     $chunk=if($c.chunk_size){[int]$c.chunk_size}else{$bytes.Length}
     for($offset=0;$offset -lt $bytes.Length;$offset+=$chunk){
      $count=[Math]::Min($chunk,$bytes.Length-$offset);$p.Write($bytes,$offset,$count)
      if($c.delay_ms -and ($offset+$count -lt $bytes.Length)){Start-Sleep -Milliseconds ([int]$c.delay_ms)}
     }
     $responses=@();for($n=0;$n -lt [int]$c.replies;$n++){$responses+=$p.ReadLine()}
     [Console]::WriteLine((@{bridge='exchange';lines=$responses}|ConvertTo-Json -Depth 4 -Compress))
    } else {throw 'Unknown bridge action'}
   } else {$p.WriteLine($line);[Console]::WriteLine($p.ReadLine())}
  } catch {[Console]::WriteLine((@{bridge_error=$_.Exception.Message}|ConvertTo-Json -Compress))}
 }
} catch {[Console]::WriteLine((@{bridge_error=$_.Exception.Message}|ConvertTo-Json -Compress))}
finally {if($p.IsOpen){$p.Close()};$p.Dispose()}
