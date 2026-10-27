static lv_color_t frame[480*480];
static lv_indev_data_t simulatedInput={};
static void readInput(lv_indev_drv_t*,lv_indev_data_t *data){*data=simulatedInput;}
static void touchAt(int x,int y,bool pressed){
 touchDown=pressed;
 simulatedInput.point={(lv_coord_t)x,(lv_coord_t)y};simulatedInput.state=pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
 simulatedUs+=40000;lv_tick_inc(40);lv_timer_handler();
 if(motionOnline)feedDiceMotion(0,0,1,millis());
}
static void click(int x,int y){auto p=orbit_pixels::rotateToPanel(x,y,pageRotation);touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);}
static void save(const char* path){
 lv_obj_invalidate(screen);lv_refr_now(nullptr);
 FILE *f=fopen(path,"wb");fprintf(f,"P6\n480 480\n255\n");
 for(auto c:frame){uint32_t rgb=lv_color_to32(c);fputc(rgb>>16&255,f);fputc(rgb>>8&255,f);fputc(rgb&255,f);}fclose(f);
}
static void openMenu(){
 auto p=orbit_pixels::rotateToPanel(240,390,pageRotation);
 touchAt(p.x,p.y,true);for(int i=0;i<20;++i)touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);
 assert(settingsPanel&&!orientationCalibrating&&!classicCharacter);
}
static void advanceDice(uint32_t ms,bool sampleMotion=true) {
 simulatedUs+=uint64_t(ms)*1000;lv_tick_inc(ms);
 if(sampleMotion&&motionOnline)feedDiceMotion(0,0,1,millis());
 diceAnimationTick(nullptr);lv_timer_handler();
}
static void keyTap() {
 handleUserButton(true,millis());advanceDice(40);handleUserButton(true,millis());
 advanceDice(70);handleUserButton(false,millis());advanceDice(40);handleUserButton(false,millis());advanceDice(40);
}
static void keyClick() { keyTap();advanceDice(320);handleUserButton(false,millis());advanceDice(40); }
static void keyDouble() { keyTap();keyTap();advanceDice(40); }
static void gameLongPress() {
 auto p=orbit_pixels::rotateToPanel(240,220,pageRotation);
 touchAt(p.x,p.y,true);for(int i=0;i<20;++i)touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);
 assert(diceSettingsPanel);
}
static void gameMotion(float x,float y,float z,uint32_t ms=50) {
 advanceDice(ms,false);feedDiceMotion(x,y,z,millis());
}
static void settleDice() { for(int i=0;i<14;++i)gameMotion(0,0,1);assert(diceGame.armed()); }
static void shakeDice() {
 gameMotion(.55f,0,1,100);gameMotion(-.55f,0,1,100);assert(diceGame.rolling());
}
static unsigned diceRandomCalls=0;static uint8_t savedCount=0,savedFaces=0;static bool diceSaveOK=true;
static uint16_t savedSandSeconds=0;static bool sandSaveOK=true;
static uint8_t savedPlayers=0;static bool playerSaveOK=true;static unsigned playerRandomCalls=0;
int main(){
 setenv("TZ","CST-8",1);tzset();
 struct tm date={};date.tm_year=126;date.tm_mon=9;date.tm_mday=23;date.tm_hour=14;date.tm_min=54;
 currentEpoch=mktime(&date);
 lv_init();
#if LV_USE_LOG
 lv_log_register_print_cb([](const char* s){fputs(s,stderr);if(strstr(s,"Error")||strstr(s,"Assert"))exit(2);});
#endif
 static lv_color_t buffer[480*24];static lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,nullptr,480*24);
 static lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=480;d.ver_res=480;d.draw_buf=&db;
 d.flush_cb=flushDisplay;d.rounder_cb=roundArea;d.monitor_cb=monitorDisplay;lv_disp_drv_register(&d);
 static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=readInput;lv_indev_drv_register(&input);
 simulatedAudio.enable();usbTimeSet=true;buildUi();refreshUi(nullptr);lv_obj_update_layout(screen);save("retro-clock.ppm");
 assert(!automaticOrientation&&pageRotation==0&&strcmp(dotTime,"14:54")==0);
 assert(timeDotCount>200&&retro_matrix.header.w==480&&retro_character.header.h==320);
 const auto initialDots=timeDotCount;
 click(240,390);assert(!hour24&&strcmp(dotTime,"02:54")==0&&audioStarts==1);
 click(240,390);assert(hour24&&strcmp(dotTime,"14:54")==0&&timeDotCount==initialDots);
 // A tap animates only the character, with exact return to its original position.
 click(240,200);assert(bouncing&&classicCharacter&&lv_img_get_src(characterImage)==&retro_character_classic);
 const auto start=simulatedUs;
 for(int i=0;i<6;++i){simulatedUs=start+i*60000;pixelAnimationTick(nullptr);
   char file[50];snprintf(file,sizeof(file),"retro-bounce-%02d.ppm",i);save(file);
 }
 assert(!bouncing&&lv_obj_get_y(characterImage)==10);
 save("retro-clock-classic.ppm");
 // Switching must leave the displayed time unchanged and be reversible.
 assert(strcmp(dotTime,"14:54")==0);
 click(240,200);assert(!classicCharacter&&lv_img_get_src(characterImage)==&retro_character);
 click(40,240);assert(classicCharacter);click(40,240);assert(!classicCharacter);
 // Rapid repeated taps still end in exactly the requested form and resting pose.
 for(int i=0;i<12;++i)click(240,200);
 assert(!classicCharacter);simulatedUs+=300000;pixelAnimationTick(nullptr);
 assert(!bouncing&&lv_obj_get_y(characterImage)==10);
 assert(audioStarts==1); // All rapid taps during this playback were discarded.
 simulatedAudio.finish();click(40,240);assert(classicCharacter&&audioStarts==2);
 click(40,240);assert(!classicCharacter&&audioStarts==2);simulatedAudio.finish();
 simulatedUs+=300000;pixelAnimationTick(nullptr);
 const auto beforeMenuAudio=audioAttempts;
 openMenu();save("retro-settings.ppm");click(40,240);assert(!classicCharacter);click(240,188);assert(focusMode&&!settingsPanel);
 click(40,240);assert(!classicCharacter);
 lv_event_send(plusButton,LV_EVENT_SHORT_CLICKED,nullptr);assert(focusMinutes==26);
 lv_event_send(plusButton,LV_EVENT_LONG_PRESSED_REPEAT,nullptr);assert(focusMinutes==31);
 setFocusMinutes(0);assert(focusMinutes==1&&lv_obj_has_state(minusButton,LV_STATE_DISABLED));
 setFocusMinutes(181);assert(focusMinutes==180&&lv_obj_has_state(plusButton,LV_STATE_DISABLED));
 setFocusMinutes(12);refreshUi(nullptr);save("retro-focus.ppm");
 onStart(nullptr);setFocusMinutes(15);assert(focusMinutes==12);
 simulatedUs+=1500000;refreshUi(nullptr);assert(strcmp(lv_label_get_text(focusLabel),"11:59")==0);
 onStart(nullptr);const auto paused=remainingUs;simulatedUs+=2000000;refreshUi(nullptr);assert(remainingUs==paused);
 onStart(nullptr);onMode(nullptr);simulatedUs+=1000000;refreshUi(nullptr);assert(remainingUs<paused&&!focusMode);
 onMode(nullptr);onReset(nullptr);onStart(nullptr);simulatedUs+=focusDurationUs+1;refreshUi(nullptr);
 assert(!focusRunning&&remainingUs==0);onReset(nullptr);onMode(nullptr);assert(!focusMode&&audioAttempts==beforeMenuAudio);
 // All software rotations and transformed hit points remain consistent.
 save("retro-rotation-0.ppm");static lv_color_t expected[480*480];memcpy(expected,frame,sizeof(frame));
 for(int r=1;r<4;++r){setPageRotation(r);char p[45];snprintf(p,sizeof(p),"retro-rotation-%d.ppm",r);save(p);
   for(int y=0;y<480;++y)for(int x=0;x<480;++x){auto a=orbit_pixels::rotateToPanel(x,y,r);assert(frame[a.y*480+a.x].full==expected[y*480+x].full);}
   const bool old=hour24;click(240,390);assert(hour24!=old&&!classicCharacter);click(240,390);assert(hour24==old&&!classicCharacter);
   click(240,200);assert(classicCharacter);click(40,240);assert(!classicCharacter);
   simulatedUs+=300000;pixelAnimationTick(nullptr);
 }
 setPageRotation(0);openMenu();click(240,254);assert(orientationCalibrating&&!settingsPanel);
 save("retro-calibration.ppm");
 auto feed=[](float x,float y,float z){for(int i=0;i<26;++i){simulatedUs+=50000;pageOrientation.updateGyro(0,0,0,millis());pageOrientation.updateAccel(x,y,z,millis());}};
 click(240,280);assert(calibrationStep==0);
 feed(0,1,0);click(240,280);assert(calibrationStep==1);
 feed(-1,0,0);click(240,280);assert(!orientationCalibrating&&automaticOrientation&&pageRotation==1&&pageOrientation.calibrated());
 openMenu();click(240,320);assert(!automaticOrientation&&!settingsPanel&&pageRotation==1);
 // The physical key uses the same debouncer and page dispatch as firmware.
 setPageRotation(0);userButton.begin(false,millis());
 diceRandomSource=+[]()->uint32_t { ++diceRandomCalls;return 0x12345678U+diceRandomCalls*3571U; };
 storeDiceSettings=+[](uint8_t count,uint8_t faces){if(!diceSaveOK)return false;savedCount=count;savedFaces=faces;return true;};
 storeSandSeconds=+[](uint16_t seconds){if(!sandSaveOK)return false;savedSandSeconds=seconds;return true;};
 storePlayerCount=+[](uint8_t players){if(!playerSaveOK)return false;savedPlayers=players;return true;};
 lv_mem_monitor_t beforeGame;lv_mem_monitor(&beforeGame);const unsigned gameAudio=audioAttempts;
 keyClick();assert(diceMode&&dicePanel&&!focusMode&&diceGame.count()==1&&diceGame.faces()==6);
 save("dice-ready.ppm");settleDice();click(240,220);assert(!diceGame.rolling()&&audioAttempts==gameAudio);
 // A long drag cannot open the dice settings.
 touchAt(240,220,true);for(int i=0;i<22;++i)touchAt(320,250,true);touchAt(320,250,false);assert(!diceSettingsPanel);
 gameLongPress();click(348,176);click(348,176);click(348,176);assert(draftDiceCount==3);
 click(348,276);click(348,276);click(348,276);assert(draftDiceFaces==8);
 assert(lv_obj_has_state(diceCountPlus,LV_STATE_DISABLED)&&lv_obj_has_state(diceFacesPlus,LV_STATE_DISABLED));
 save("dice-settings.ppm");click(312,386);advanceDice(40);
 assert(!diceSettingsPanel&&diceGame.count()==3&&diceGame.faces()==8&&savedCount==3&&savedFaces==8);
 save("dice-ready-3.ppm");
 settleDice();shakeDice();const uint32_t rollStart=millis();const unsigned randomBefore=diceRandomCalls;
 static lv_color_t dicePartial[480*480];
 for(int i=0;i<50;++i) {
   if(i)advanceDice(40);
   feedDiceMotion(i%2?1:-1,0,1,millis());
   assert(diceGame.rolling()&&diceRandomCalls==randomBefore);
   assert(diceFrame.count<=dice_view::MAX_POLYGONS&&!diceFrame.overflow);
   for(unsigned p=0;p<diceFrame.count;++p) {
     const auto &bounds=diceFrame.shapes[p].bounds;
     assert(bounds.x1>=40&&bounds.x2<440&&bounds.y1>=110&&bounds.y2<334);
   }
   lv_refr_now(nullptr);memcpy(dicePartial,frame,sizeof(frame));
   char file[48];snprintf(file,sizeof(file),"dice-roll-%02d.ppm",i);save(file);
   assert(memcmp(dicePartial,frame,sizeof(frame))==0); // No trails after partial redraw.
 }
 assert(uint32_t(millis()-rollStart)==1960);advanceDice(40);
 assert(diceGame.hasResult()&&!diceGame.rolling()&&diceRandomCalls==randomBefore+3);
 char total[8];snprintf(total,sizeof(total),"%u",diceGame.total());assert(strcmp(lv_label_get_text(diceTotal),total)==0);
 save("dice-result.ppm");const auto result=diceGame.total();
 lv_area_t totalBounds,hintBounds,canvasBounds;
 lv_obj_get_coords(diceTotal,&totalBounds);lv_obj_get_coords(diceHint,&hintBounds);lv_obj_get_coords(diceCanvas,&canvasBounds);
 assert(totalBounds.y1==376&&lv_obj_get_style_text_font(diceTotal,LV_PART_MAIN)==&lv_font_montserrat_48);
 assert(canvasBounds.y2<hintBounds.y1&&hintBounds.y2<356);

 // Custom dice drawing and the long-press settings keep the same four-way mapping.
 static lv_color_t gameExpected[480*480];memcpy(gameExpected,frame,sizeof(frame));
 for(int r=1;r<4;++r) {
   setPageRotation(r);save("dice-rotation.ppm");
   for(int y=0;y<480;++y)for(int x=0;x<480;++x) {
     auto p=orbit_pixels::rotateToPanel(x,y,r);assert(frame[p.y*480+p.x].full==gameExpected[y*480+x].full);
   }
   gameLongPress();click(168,386);advanceDice(40);assert(!diceSettingsPanel&&diceGame.count()==3);
 }
 setPageRotation(0);
 for(int i=0;i<12;++i)gameMotion(i%2?1:-1,0,1,100);
 assert(diceGame.hasResult()&&!diceGame.rolling()&&diceGame.total()==result);
 // Cancel keeps the configuration; failed persistence stays in the dialog.
 gameLongPress();click(132,176);assert(draftDiceCount==2);click(168,386);advanceDice(40);assert(diceGame.count()==3);
 gameLongPress();click(132,276);diceSaveOK=false;click(312,386);assert(diceSettingsPanel&&diceGame.faces()==8);
 diceSaveOK=true;click(312,386);advanceDice(40);assert(!diceSettingsPanel&&diceGame.faces()==7);
 // Sweep all eighteen configurations at 10 ms to catch projection clipping,
 // polygon overflow and depth-dependent bounds between the captured 40 ms frames.
 unsigned largestDiceFrame=0;
 for(int count=1;count<=3;++count)for(int faces=2;faces<=8;++faces) {
   if(faces==2&&count!=1)continue;
   dice_game::Game probe;assert(probe.configure(count,faces));probe.enter();uint32_t stamp=0;
   for(int j=0;j<14;++j){stamp+=50;probe.sample(0,0,1,stamp);}
   stamp+=100;probe.sample(.55f,0,1,stamp);
   stamp+=100;assert(probe.sample(-.55f,0,1,stamp));
   static dice_view::Frame projected;
   for(int elapsed=0;elapsed<2000;elapsed+=10) {
     dice_view::build(projected,probe,stamp+elapsed);
     assert(!projected.overflow&&projected.count>0);
     largestDiceFrame=std::max(largestDiceFrame,projected.count);
     for(unsigned j=0;j<projected.count;++j) {
       const auto &b=projected.shapes[j].bounds;
       const auto &shape=projected.shapes[j];assert(shape.vertices>=3&&shape.vertices<=8);
       for(int k=0;k<shape.vertices;++k) {
         const auto a=shape.points[k],p=shape.points[(k+1)%shape.vertices],c=shape.points[(k+2)%shape.vertices];
         assert(int32_t(p.x-a.x)*(c.y-p.y)-int32_t(p.y-a.y)*(c.x-p.x)>0);
       }
       assert(b.x1>=40&&b.x2<440&&b.y1>=110&&b.y2<334);
     }
   }
 }
 for(int count=1;count<=3;++count)for(int faces=2;faces<=8;++faces) {
   if(faces==2&&count!=1)continue;
   for(int value=1;value<=faces;++value) {
     dice_game::Game completed;completed.configure(count,faces);completed.enter();
     uint32_t stamp=0;completed.sample(0,0,1,stamp);stamp+=100;completed.sample(.55f,0,1,stamp);stamp+=100;
     assert(completed.sample(-.55f,0,1,stamp));
     auto fixed=[&]()->uint32_t{return uint32_t(faces*1000+value-1);};
     assert(completed.tick(stamp+2000,fixed));static dice_view::Frame finalFrame;
     dice_view::build(finalFrame,completed,stamp+2000);
     assert(!finalFrame.overflow);largestDiceFrame=std::max(largestDiceFrame,finalFrame.count);
     for(unsigned i=0;i<finalFrame.count;++i) {
       const auto &b=finalFrame.shapes[i].bounds;assert(b.x1>=40&&b.x2<440&&b.y1>=110&&b.y2<334);
     }
   }
 }
 for(const auto &values:dice_view::D6_VALUES) {
   assert(values[0]+values[1]==7&&values[2]+values[3]==7&&values[4]+values[5]==7);
   unsigned mask=0;for(auto v:values)mask|=1U<<v;assert(mask==126);
 }
 printf("3D dice projection: all 19 configurations stay inside canvas; peak %u/%u polygons\n",largestDiceFrame,dice_view::MAX_POLYGONS);
 // Two dice and one die both fit the viewport, including their rotations.
 for(int count=1;count<=2;++count) {
   diceGame.configure(count,6);rebuildDiceFrame();updateDiceLabels();
   feedDiceMotion(0,0,1,millis());updateDiceLabels();
   if(count==1)save("dice-ready-1.ppm");
   settleDice();shakeDice();
   for(int i=0;i<50;++i) {
     if(count==1) { char file[48];snprintf(file,sizeof(file),"dice-one-roll-%02d.ppm",i);save(file); }
     feedDiceMotion(0,0,1,millis());advanceDice(40);for(unsigned p=0;p<diceFrame.count;++p) {
     const auto &b=diceFrame.shapes[p].bounds;assert(b.x1>=40&&b.x2<440&&b.y1>=110&&b.y2<334);
   }}
   char file[48];snprintf(file,sizeof(file),"dice-result-%d.ppm",count);save(file);
   if(count==1) {
     const auto pose=dice_view::rotation(-.34f,-.43f,0);
     const auto fit=dice_view::layout(pose,6,1,0,0);
     for(const auto &source:dice_view::PLANES) {
       const dice_view::Plane plane={pose.apply(source.normal),pose.apply(source.u),pose.apply(source.v)};
       if(plane.normal.z*6<=1)continue;
       for(float u:{-.86f,-.5f,0.f,.5f,.86f})for(float v:{-.86f,-.5f,0.f,.5f,.86f}) {
         const auto pt=fit.view.point(plane.center+plane.u*u+plane.v*v);
         assert(frame[pt.y*480+pt.x].full!=lv_color_hex(0x111C2B).full);
       }
     }
   }
 }
 // Capture each actual selectable solid, including a full eight-face roll.
 for(int faces=3;faces<=8;++faces) {
   diceGame.configure(1,faces);feedDiceMotion(0,0,1,millis());rebuildDiceFrame();updateDiceLabels();
   if(faces==8)save("dice-octa-ready.ppm");
   shakeDice();
   for(int i=0;i<50;++i) {
     if(faces==8) { char file[48];snprintf(file,sizeof(file),"dice-octa-roll-%02d.ppm",i);save(file); }
     feedDiceMotion(0,0,1,millis());advanceDice(40);
   }
   char file[48];snprintf(file,sizeof(file),"dice-shape-%d.ppm",faces);save(file);
 }
 // D2 settings normalize the count and prevent even a forced plus event.
 diceGame.configure(3,8);rebuildDiceFrame();updateDiceLabels();gameLongPress();
 for(int i=0;i<6;++i)click(132,276);
 assert(draftDiceFaces==2&&draftDiceCount==1);
 assert(lv_obj_has_state(diceCountMinus,LV_STATE_DISABLED)&&lv_obj_has_state(diceCountPlus,LV_STATE_DISABLED));
 lv_event_send(diceCountPlus,LV_EVENT_SHORT_CLICKED,nullptr);assert(draftDiceCount==1);
 save("coin-settings.ppm");click(312,386);advanceDice(40);
 assert(diceGame.faces()==2&&diceGame.count()==1&&savedCount==1&&savedFaces==2);
 save("coin-ready.ppm");
 auto previousRandom=diceRandomSource;diceRandomSource=+[]()->uint32_t{return 0;};
 shakeDice();
 for(int i=0;i<50;++i) {
   char file[48];snprintf(file,sizeof(file),"coin-roll-%02d.ppm",i);save(file);
   assert(!diceFrame.overflow);for(unsigned j=0;j<diceFrame.count;++j) {
     const auto &b=diceFrame.shapes[j].bounds;assert(b.x1>=40&&b.x2<440&&b.y1>=110&&b.y2<334);
   }
   advanceDice(40);
 }
 assert(diceGame.total()==1&&strcmp(lv_label_get_text(diceTotal),"正面")==0);save("coin-head.ppm");
 settleDice();diceRandomSource=+[]()->uint32_t{return 1;};shakeDice();advanceDice(2000);
 assert(diceGame.total()==2&&strcmp(lv_label_get_text(diceTotal),"反面")==0);save("coin-tail.ppm");
 diceRandomSource=previousRandom;
 // A double tap stays in the dice page until it can route directly to sand.
 keyDouble();assert(diceMode&&sandMode&&sandPanel&&!sandSettingsPanel);
 assert(sandTimer.seconds()==60&&sandTimer.state()==sand_timer::State::Idle);
 assert(strcmp(lv_label_get_text(sandValue),"01:00")==0);save("sand-ready.ppm");
 auto checkGrains=[](){
   assert(sand_view::seatsReady&&!sand_view::grains.overflow);
   assert(sand_view::grains.count==sand_view::BED_GRAINS);
   for(unsigned i=0;i<sand_view::grains.count;++i){const auto &g=sand_view::grains.points[i];
     assert(g.ink<10&&sand_view::insideGlass(g.x,g.y,g.size));
   }
 };
 // Check every bed level, including the narrow-neck stream, before exercising UI.
 sand_timer::Countdown scan;scan.configure(460);scan.toggle(0);
 for(unsigned i=0;i<=460;++i){scan.update(i*1000);sand_view::build(diceFrame,scan,i*1000);checkGrains();assert(!diceFrame.overflow);}
 rebuildSandFrame();
 click(172,412);assert(sandTimer.state()==sand_timer::State::Running);advanceDice(1550);
 assert(sandTimer.remainingMs()==58450);assert(strcmp(lv_label_get_text(sandValue),"00:59")==0);checkGrains();save("sand-running.ppm");
 assert(!diceFrame.overflow);for(unsigned j=0;j<diceFrame.count;++j) {
   const auto &b=diceFrame.shapes[j].bounds;assert(b.x1>=110&&b.x2<370&&b.y1>=92&&b.y2<322);
 }
 click(172,412);assert(sandTimer.state()==sand_timer::State::Paused);
 const auto sandPaused=sandTimer.remainingMs();
 const auto pausedGrains=sand_view::grains;advanceDice(2500);assert(sandTimer.remainingMs()==sandPaused);
 rebuildSandFrame();assert(memcmp(&pausedGrains,&sand_view::grains,sizeof(pausedGrains))==0);
 click(172,412);advanceDice(800);assert(sandTimer.remainingMs()<sandPaused);
 auto sandLongPress=[](){
   auto p=orbit_pixels::rotateToPanel(240,220,pageRotation);
   touchAt(p.x,p.y,true);for(int i=0;i<20;++i)touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);assert(sandSettingsPanel);
 };
 sandLongPress();assert(sandTimer.state()==sand_timer::State::Paused);save("sand-settings.ppm");
 click(312,238);click(312,298);assert(draftSandSeconds==71);
 lv_event_send(sandAdjustButtons[3],LV_EVENT_LONG_PRESSED_REPEAT,nullptr);assert(draftSandSeconds==131);
 const auto beforeCancel=sandTimer.remainingMs();click(168,410);advanceDice(40);
 assert(!sandSettingsPanel&&sandTimer.seconds()==60&&sandTimer.remainingMs()==beforeCancel&&savedSandSeconds==0);
 sandLongPress();click(144,350);assert(draftSandSeconds==30);sandSaveOK=false;click(312,410);
 assert(sandSettingsPanel&&sandTimer.seconds()==60);sandSaveOK=true;click(312,410);advanceDice(40);
 assert(!sandSettingsPanel&&savedSandSeconds==30&&sandTimer.remainingSeconds()==30&&sandTimer.state()==sand_timer::State::Idle);
 sandLongPress();draftSandSeconds=1;updateSandDraft();
 assert(lv_obj_has_state(sandAdjustButtons[0],LV_STATE_DISABLED)&&lv_obj_has_state(sandAdjustButtons[2],LV_STATE_DISABLED));
 draftSandSeconds=3600;updateSandDraft();assert(lv_obj_has_state(sandAdjustButtons[1],LV_STATE_DISABLED));
 sandTimer.configure(3600);updateSandLabels();assert(strcmp(lv_label_get_text(sandValue),"60:00")==0);
 draftSandSeconds=1;updateSandDraft();click(312,410);advanceDice(40);click(172,412);
 advanceDice(999);assert(sandTimer.remainingMs()==1);advanceDice(1);assert(sandTimer.state()==sand_timer::State::Done);
 assert(strcmp(lv_label_get_text(sandValue),"00:00")==0);checkGrains();save("sand-done.ppm");click(172,412);assert(sandTimer.state()==sand_timer::State::Running);
 keyClick();assert(diceMode&&!sandMode&&!sandPanel&&sandTimer.state()==sand_timer::State::Paused);
 const auto hiddenSand=sandTimer.remainingMs();advanceDice(5000);assert(sandTimer.remainingMs()==hiddenSand);
 keyTap();assert(diceMode&&!sandMode);advanceDice(320);handleUserButton(false,millis());advanceDice(40);assert(!diceMode);
 keyClick();assert(diceMode);keyDouble();assert(sandMode&&sandTimer.remainingMs()==hiddenSand);
 // Physical coordinate transforms apply to the hourglass and its settings too.
 sandTimer.configure(60);rebuildSandFrame();updateSandLabels();save("sand-rotation.ppm");
 static lv_color_t sandExpected[480*480];memcpy(sandExpected,frame,sizeof(frame));
 for(int r=1;r<4;++r) {
   setPageRotation(r);save("sand-rotation.ppm");
   for(int y=0;y<480;++y)for(int x=0;x<480;++x) {
     auto p=orbit_pixels::rotateToPanel(x,y,r);assert(frame[p.y*480+p.x].full==sandExpected[y*480+x].full);
   }
   sandLongPress();click(168,410);advanceDice(40);assert(!sandSettingsPanel);
 }
 setPageRotation(0);
 // Six seconds of actual animation, retaining partial-refresh equivalence.
 sandTimer.configure(6);rebuildSandFrame();updateSandLabels();save("sand-demo-ready.ppm");click(172,412);
 for(int i=0;i<60;++i) {
   if(i)advanceDice(100);checkGrains();
   lv_refr_now(nullptr);memcpy(dicePartial,frame,sizeof(frame));
   char file[48];snprintf(file,sizeof(file),"sand-flow-%02d.ppm",i);save(file);
   assert(memcmp(dicePartial,frame,sizeof(frame))==0);
 }
 advanceDice(100);assert(sandTimer.state()==sand_timer::State::Done);save("sand-demo-done.ppm");
 // Sand double-click routes to the pixel wheel without briefly returning to dice.
 sandTimer.configure(60);rebuildSandFrame();updateSandLabels();click(172,412);advanceDice(500);
 keyDouble();assert(playerMode&&sandMode&&diceMode&&playerPanel&&sandTimer.state()==sand_timer::State::Paused);
 const auto sandBehindPlayer=sandTimer.remainingMs();assert(playerGame.players()==4&&!playerGame.spinning());
 save("player-ready.ppm");
 const auto playerRandomSource=diceRandomSource;diceRandomSource=+[]()->uint32_t{++playerRandomCalls;return 101U;};
 click(240,220);assert(playerGame.spinning()&&playerRandomCalls==1);
 lv_event_send(playerPanel,LV_EVENT_SHORT_CLICKED,nullptr);assert(playerRandomCalls==1&&playerGame.winner()==0);
 for(int i=0;i<105;++i) {
   if(i)advanceDice(40);
   assert(playerGame.spinning()&&playerGame.winner()==0);
   if(i==74)assert(!playerGame.slowing(millis()));if(i==75)assert(playerGame.slowing(millis()));
   lv_refr_now(nullptr);memcpy(dicePartial,frame,sizeof(frame));
   char name[48];snprintf(name,sizeof(name),"player-spin-%03d.ppm",i);save(name);
   assert(memcmp(dicePartial,frame,sizeof(frame))==0);
 }
 advanceDice(40);assert(!playerGame.spinning()&&playerGame.winner()==2);
 assert(strcmp(lv_label_get_text(playerResult),"玩家 2 先开始")==0&&sandTimer.remainingMs()==sandBehindPlayer);
 save("player-result.ppm");
 auto playerLongPress=[](){auto p=orbit_pixels::rotateToPanel(240,220,pageRotation);
   touchAt(p.x,p.y,true);for(int i=0;i<20;++i)touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);assert(playerSettingsPanel);};
 playerLongPress();click(348,190);click(348,190);assert(draftPlayers==6);save("player-settings.ppm");
 click(168,410);advanceDice(40);assert(!playerSettingsPanel&&playerGame.players()==4&&playerGame.winner()==2&&savedPlayers==0);
 playerLongPress();draftPlayers=2;updatePlayerDraft();assert(lv_obj_has_state(playerMinus,LV_STATE_DISABLED));
 lv_event_send(playerMinus,LV_EVENT_SHORT_CLICKED,nullptr);assert(draftPlayers==2);
 draftPlayers=12;updatePlayerDraft();assert(lv_obj_has_state(playerPlus,LV_STATE_DISABLED));
 lv_event_send(playerPlus,LV_EVENT_SHORT_CLICKED,nullptr);assert(draftPlayers==12);
 click(276,310);assert(draftPlayers==6);playerSaveOK=false;click(312,410);assert(playerSettingsPanel&&playerGame.players()==4);
 playerSaveOK=true;click(312,410);advanceDice(40);assert(!playerSettingsPanel&&playerGame.players()==6&&savedPlayers==6&&playerGame.winner()==0);
 // Sector grids stay bounded for the entire supported range; labels fit the wheel.
 for(int players=2;players<=12;++players) {
   player_game::Game probe;probe.configure(players);player_view::build(probe,0);
   for(int i=0;i<player_view::SIZE*player_view::SIZE;++i){assert(player_view::pixels[i]<19);if(!player_view::base[i])assert(!player_view::pixels[i]);}
   for(int i=0;i<players;++i){const auto p=player_view::labelPositions[i];assert(p.x>=140&&p.x<=340&&p.y>=130&&p.y<=320);}
 }
 rebuildPlayerFrame();updatePlayerLabels();save("player-rotation.ppm");
 static lv_color_t playerExpected[480*480];memcpy(playerExpected,frame,sizeof(frame));
 for(int r=1;r<4;++r) {
   setPageRotation(r);save("player-rotation.ppm");
   for(int y=0;y<480;++y)for(int x=0;x<480;++x){const auto p=orbit_pixels::rotateToPanel(x,y,r);assert(frame[p.y*480+p.x].full==playerExpected[y*480+x].full);}
   playerLongPress();click(168,410);advanceDice(40);assert(!playerSettingsPanel);
 }
 setPageRotation(0);click(240,220);advanceDice(500);assert(playerGame.spinning());playerLongPress();assert(!playerGame.spinning());
 keyTap();assert(!playerMode&&sandMode&&!playerPanel&&!playerSettingsPanel);
 advanceDice(5000);assert(playerGame.winner()==0&&sandTimer.remainingMs()==sandBehindPlayer);
 lv_mem_monitor_t beforePlayerLoops;lv_mem_monitor(&beforePlayerLoops);
 for(int i=0;i<12;++i) {
   keyDouble();assert(playerMode);if(i%2==0)playerLongPress();else {click(240,220);assert(playerGame.spinning());}
   keyTap();assert(!playerMode&&sandMode&&!playerPanel&&!playerSettingsPanel&&!playerGame.spinning());
 }
 lv_mem_monitor_t afterPlayerLoops;lv_mem_monitor(&afterPlayerLoops);
 assert(afterPlayerLoops.free_size+256>=beforePlayerLoops.free_size&&audioAttempts==gameAudio);
 diceRandomSource=playerRandomSource;
 keyTap();assert(sandMode&&!playerMode);advanceDice(320);handleUserButton(false,millis());advanceDice(40);
 assert(diceMode&&!sandMode);sandTimer.configure(60);
 printf("PASS: pixel wheel, 3-second fast phase plus slowdown, settings/save/cancel, Key3 hierarchy, four rotations, partial redraw, all player counts and repeated page memory\n");
 lv_mem_monitor_t beforeSandLoops;lv_mem_monitor(&beforeSandLoops);
 for(int i=0;i<12;++i) {
   keyDouble();assert(sandMode);if(i%2==0)sandLongPress();keyClick();assert(diceMode&&!sandMode&&!sandSettingsPanel);
 }
 lv_mem_monitor_t afterSandLoops;lv_mem_monitor(&afterSandLoops);
 assert(afterSandLoops.free_size+256>=beforeSandLoops.free_size&&audioAttempts==gameAudio);
 printf("PASS: D2 count lock, heads/tails, Key3 single/double routes, sand timing/settings, four rotations, partial redraw and repeated page memory\n");
 // A stale stream has an explicit hint, and new acceleration clears it.
 advanceDice(800,false);assert(strcmp(lv_label_get_text(diceHint),"未收到运动数据")==0);
 feedDiceMotion(0,0,1,millis());updateDiceLabels();assert(strcmp(lv_label_get_text(diceHint),"未收到运动数据")!=0);
 settleDice();shakeDice();keyClick();assert(!diceMode&&!dicePanel&&!diceGame.rolling());
 // Focus keeps counting in the background and the key always returns to the clock.
 onMode(nullptr);onStart(nullptr);const uint64_t focusBefore=remainingUs;
 keyClick();assert(diceMode&&!focusMode&&focusRunning);advanceDice(3000);keyClick();assert(!diceMode&&!focusMode&&remainingUs<focusBefore);onReset(nullptr);
 onSettingsOpen(nullptr);assert(settingsPanel);keyClick();assert(diceMode&&!settingsPanel);keyClick();assert(!diceMode);
 for(int i=0;i<20;++i){keyClick();assert(diceMode);keyClick();assert(!diceMode);}
 advanceDice(1000);lv_mem_monitor_t afterGame;lv_mem_monitor(&afterGame);
 assert(afterGame.free_size+256>=beforeGame.free_size&&audioAttempts==gameAudio);
 unsigned longBars=0;for(auto row:COIN_GLYPHS[0])if(__builtin_popcount(row)>=8)++longBars;assert(longBars>=2);
 for(uint32_t point:{uint32_t(0x6B63),uint32_t(0x53CD),uint32_t(0x9762)}) { lv_font_glyph_dsc_t g={};assert(lv_font_get_glyph_dsc(&orbit_font_zh_32,&g,point,0)&&!g.is_placeholder); }
 // Every displayed CJK glyph must exist in the generated 16/20px subsets.
 FONT_CHECK
 lv_mem_monitor_t mem;lv_mem_monitor(&mem);
 printf("PASS: clock/audio/focus/rotation plus Key3 dice entry/exit, shake, 2-second animation, settings/save/cancel, 1-3 dice bounds, repeat entry memory and CJK glyphs; LVGL free=%u bytes\n",(unsigned)mem.free_size);
}
