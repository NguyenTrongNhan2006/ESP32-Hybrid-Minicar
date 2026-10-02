// Self-contained SVG diagrams; no CDN, renderer service, or runtime dependency.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const out = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../schematic');
const e = value => String(value).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;');
const text = (x,y,value,size=18,color='#20364b',anchor='middle') => `<text x="${x}" y="${y}" text-anchor="${anchor}" font-size="${size}" fill="${color}">${e(value)}</text>`;
const arrow = (d,color='#507089',dash=false) => `<path d="${d}" fill="none" stroke="${color}" stroke-width="2.5" ${dash?'stroke-dasharray="8 5"':''} marker-end="url(#arrow)"/>`;
const box = (x,y,w,h,title,lines=[],tone='blue') => {
  const colors={blue:['#eaf2f9','#5782a3'],red:['#fcecee','#ba4658'],green:['#e7f6ed','#358666'],yellow:['#fff4d9','#ad873a'],grey:['#eef1f4','#7b8795']};
  const [fill,stroke]=colors[tone];
  return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="14" fill="${fill}" stroke="${stroke}" stroke-width="2"/>`+text(x+w/2,y+33,title,21,stroke)+lines.map((line,i)=>text(x+w/2,y+63+i*25,line,17)).join('');
};
function write(name,w,h,title,subtitle,body){
  const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" role="img" aria-label="${e(title)}"><defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 z" fill="#507089"/></marker></defs><rect width="${w}" height="${h}" fill="#fff"/><g font-family="Segoe UI,Arial,sans-serif">${text(40,48,title,30,'#132e43','start')}${text(40,81,subtitle,17,'#547083','start')}${body}${text(40,h-24,'ESP32 Hybrid Minicar • Bản thử bàn • Chưa xác nhận vận hành xe thi',15,'#647687','start')}</g></svg>`;
  fs.writeFileSync(path.join(out,name+'.svg'),svg);
}
write('fsm',1200,590,'FSM • STOPPED / ARMED / RUNNING','GPIO giữ nguyên. Đầu ra bên dưới là đích thử, chưa chứng minh cơ cấu thật an toàn.',
  arrow('M 340 280 H 455')+arrow('M 745 280 H 860')+
  arrow('M 1005 210 V 150 H 195 V 210')+arrow('M 600 210 V 150 H 195 V 210')+
  text(600,128,'STOP / E-Stop / pin thấp / lỗi • RUNNING thêm timeout',19)+
  text(398,256,'ARM',17)+text(802,256,'RUN',17)+
  box(50,210,290,145,'STOPPED',['Ga 0° · lái 90°','ESC / GPIO26: 1000 µs','LED đỏ'],'red')+
  box(455,210,290,145,'ARMED',['Đầu ra giữ idle','RUN chỉ từ nguồn đã ARM','LED xanh'],'green')+
  box(860,210,290,145,'RUNNING',['STEER / GAS / ESC','GPIO26 test: 1500 µs','LED vàng'],'yellow')+
  text(195,390,'Boot / reset → STOPPED',18)+text(1005,390,'Lệnh hợp lệ gia hạn heartbeat',17)+
  box(50,430,1100,100,'Timeout: ESP-IDF 500 ms • Wokwi 5000 ms',[
    'Hết hạn → dừng; PING không tự chạy lại. Cần ARM rồi RUN mới. GPIO26 chỉ là PWM test.'
  ],'grey'));
write('flowchart',1200,1200,'Flowchart • vòng điều khiển ESP-IDF C','Một main task sở hữu FSM/PWM; không chờ HTTP hoặc Serial newline.',
  arrow('M 580 205 V 235')+arrow('M 860 300 H 950')+
  arrow('M 580 365 V 445')+arrow('M 1055 360 V 410 H 580 V 445')+
  arrow('M 580 535 V 585')+arrow('M 580 685 V 735')+
  arrow('M 580 845 V 885')+arrow('M 580 975 V 1035')+
  arrow('M 300 1080 H 220 V 300 H 300')+
  box(300,115,560,90,'Khởi tạo',['Đích dừng → STOPPED → Wi-Fi → watchdog'])+
  '<path d="M 580 235 L 860 300 L 580 365 L 300 300 Z" fill="#fff4d9" stroke="#ad873a" stroke-width="2"/>'+
  text(580,292,'E-Stop / ADC / pin / driver',20)+text(580,321,'hoặc timeout có lỗi?',20)+
  text(903,283,'Có',17)+text(614,397,'Không',17)+
  box(950,260,210,100,'STOPPED',['Đích dừng','Tăng epoch'],'red')+
  box(300,445,560,90,'USB Serial',['Tối đa 32 byte/lần; kiểm tra safety khi đọc'])+
  box(300,585,560,100,'Điện thoại',['Bit STOP / disconnect trước queue','Tối đa 4 lệnh; loại lệnh quá tuổi / epoch cũ'])+
  box(300,735,560,110,'Chấp nhận hoặc từ chối lệnh',['Kiểm tra safety, FSM, owner, sequence, cú pháp','Chỉ lệnh hợp lệ gia hạn heartbeat'])+
  box(300,885,560,90,'Safety và Serial lần nữa',['Cập nhật đích PWM / LED từ cùng một task'])+
  box(300,1035,560,90,'Kết thúc vòng',['TX không chờ → reset watchdog → yield 1 tick'])+
  text(110,560,'Lặp lại',19)+text(110,590,'Tick 1 ms',17)+text(110,620,'không phải',16)+text(110,645,'cam kết độ trễ',16));
write('tasks',1260,740,'Các luồng • quyền cập nhật rõ ràng','Task SDK truyền dữ liệu; ISR chỉ chốt cờ. Chỉ main task ghi FSM và đích PWM.',
  arrow('M 290 220 H 360')+arrow('M 660 220 H 735 V 355')+
  arrow('M 290 390 H 660')+arrow('M 290 550 H 590 V 430 H 660')+
  arrow('M 875 355 V 220 H 970')+arrow('M 875 445 V 565 H 970')+
  box(40,155,250,120,'HTTP / điện thoại',['POST lệnh','Không ghi PWM'])+
  box(360,155,300,120,'Queue 8 lệnh',['Seq / epoch / timestamp','Quá 100 ms: bỏ'])+
  box(40,335,250,115,'STOP / Wi-Fi',['Bit STOP ưu tiên','Disconnect báo sự kiện'],'red')+
  box(40,495,250,110,'ISR GPIO27',['Chốt cạnh E-Stop','Không xử lý FSM'],'yellow')+
  box(660,355,245,110,'Main task',['FSM + interlock','USB Serial bounded'],'green')+
  box(970,155,250,120,'LEDC / GPIO',['PWM 18 / 19 / 23 / 26','LED 25 / 32 / 33'])+
  box(970,510,250,110,'Snapshot',['Khóa ngắn khi copy','HTTP đọc trạng thái'])+
  text(720,650,'Watchdog theo dõi main task: 2 s. Không thay thế kill độc lập.',18));
write('system',1400,1120,'Sơ đồ khối và dây tín hiệu','Đường nét đứt: còn phải xác minh model/nguồn/logic. Xem PINOUT.md trước khi đấu tải.',
  arrow('M 310 180 H 550')+arrow('M 310 320 H 470 V 260 H 550')+
  arrow('M 310 470 H 550')+arrow('M 310 630 H 470 V 510 H 550')+
  arrow('M 850 180 H 1090')+arrow('M 850 255 H 1010 V 320 H 1090')+
  arrow('M 850 335 H 1050 V 465 H 1090')+arrow('M 850 420 H 990 V 610 H 1090')+
  arrow('M 850 505 H 950 V 745 H 1090')+
  arrow('M 310 865 H 550')+arrow('M 850 865 H 1025 V 190 H 1090','#507089',true)+
  arrow('M 1025 865 V 330 H 1090','#507089',true)+
  arrow('M 175 920 V 945 H 1380 V 465 H 1360','#507089',true)+
  text(960,934,'Nguồn ESC: cần xác minh model / bảo vệ',17)+
  arrow('M 850 1030 H 1090','#507089',true)+
  box(40,125,270,110,'Điện thoại',['Wi-Fi ESP32-Hybrid','Trình duyệt / ARM / giữ RUN'])+
  box(40,265,270,110,'USB DevKit',['Nguồn ESP32 khi thử bàn','Serial 115200'])+
  box(40,410,270,110,'E-Stop',['GPIO27 ↔ nút NO ↔ GND','INPUT_PULLUP'],'red')+
  box(40,565,270,110,'BAT SIM',['Biến trở 3V3 / GND','SIG → GPIO34'])+
  box(550,130,300,430,'ESP32-WROOM',['','Một task điều khiển','','GPIO18 → lái','GPIO19 → ga xăng','GPIO23 → ESC test','GPIO26 → RCEXL test','','LED25 / LED32 / LED33'],'green')+
  box(1090,130,270,105,'Servo lái',['Signal GPIO18','Nguồn ngoài theo model'])+
  box(1090,275,270,105,'Servo ga xăng',['Signal GPIO19','Hiệu chuẩn ga đóng'])+
  box(1090,420,270,105,'ESC / BLDC',['GPIO23; analyzer D0','Xác minh ESC trước tải'],'yellow')+
  box(1090,565,270,105,'Analyzer D1',['GPIO26 test ONLY','Chưa nối ignition'],'yellow')+
  box(1090,710,270,95,'Ba LED',['220–330 Ω mỗi LED'])+
  box(40,805,270,115,'LiPo 3S',['Tối đa 12.6 V','Cầu chì / phân phối theo tải'],'yellow')+
  box(550,805,300,115,'Nguồn servo',['Buck/BEC đúng model','Không song song hai đầu ra'])+
  box(550,975,300,90,'Kill độc lập',['Chưa chốt theo ignition'],'red')+
  box(1090,975,270,90,'Đánh lửa 2 kỳ',['RCEXL / loại chưa chốt'],'red')+
  text(280,746,'GND tín hiệu cần tham chiếu đúng.',18)+
  text(280,771,'Dòng tải lớn về phân phối nguồn riêng.',17)+
  text(270,1010,'GPIO34 chưa đo pin thật.',18)+text(270,1040,'Không nối LiPo vào GPIO.',18));
console.log('Wrote 4 standalone SVG diagrams.');
