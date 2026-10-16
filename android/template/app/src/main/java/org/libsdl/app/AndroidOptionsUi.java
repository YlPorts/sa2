package org.libsdl.app;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.os.Build;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Android-only overlay. The game remains the original native SDL activity. */
final class AndroidOptionsUi {
    private static final int SIZE=2, OPACITY=3, HIT=4, LAYOUT=11, COUNT=32;
    private static final int BLUE=0xff073766, WHITE=0xfff5f6ff, GOLD=0xffffd84a;
    private static final String[] LABELS={"+", "A", "B", "L", "R", "SELECT", "START"};
    private final SA2Activity activity;
    private final SharedPreferences prefs;
    private int[] values = defaults();
    private Dialog dialog;
    private int[] editingBefore;

    AndroidOptionsUi(SA2Activity activity) {
        this.activity=activity;
        prefs=activity.getSharedPreferences("sa-android-options-v1", 0);
        for(int i=0;i<COUNT;i++) values[i]=prefs.getInt("v"+i, values[i]);
        persist();
    }

    private static int[] defaults() {
        int[] v=new int[COUNT];
        int[] head={0,0,85,80,120,100,100,100,0,60,0};
        System.arraycopy(head,0,v,0,head.length);
        for(int i=0;i<7;i++){v[LAYOUT+i*3]=-1;v[LAYOUT+i*3+1]=-1;v[LAYOUT+i*3+2]=100;}
        return v;
    }

    private int dp(float n){return Math.round(n*activity.getResources().getDisplayMetrics().density);}

    void attach() {
        FrameLayout root=activity.findViewById(android.R.id.content);
        TextView gear=new TextView(activity);
        gear.setText("⋮");gear.setTextSize(26);gear.setTextColor(WHITE);
        gear.setGravity(Gravity.CENTER);gear.setContentDescription("Ajustes de Android");
        gear.setBackgroundColor(0x55001838);
        FrameLayout.LayoutParams lp=new FrameLayout.LayoutParams(dp(40),dp(40),Gravity.TOP|Gravity.RIGHT);
        lp.rightMargin=dp(6);lp.topMargin=dp(4);
        root.addView(gear,lp);gear.setOnClickListener(v->open());
    }

    private void send(boolean open) {
        try { SA2Activity.nativeOptions(values,open); }
        catch(UnsatisfiedLinkError ignored) { /* Native startup loads the saved file. */ }
    }

    private void persist() {
        SharedPreferences.Editor editor=prefs.edit();
        StringBuilder text=new StringBuilder();
        for(int i=0;i<COUNT;i++){editor.putInt("v"+i,values[i]);text.append(values[i]).append('\n');}
        editor.apply();
        File target=new File(activity.getFilesDir(),"sa-android-options.cfg");
        File temporary=new File(activity.getFilesDir(),"sa-android-options.tmp");
        try(FileOutputStream stream=new FileOutputStream(temporary)) {
            stream.write(text.toString().getBytes(StandardCharsets.UTF_8));stream.getFD().sync();
        } catch(Exception ignored) { temporary.delete();return; }
        if(!temporary.renameTo(target)) temporary.delete();
    }

    private void changed(){persist();send(true);}

    private TextView label(String text,int size){
        TextView v=new TextView(activity);v.setText(text);v.setTextSize(size);v.setTextColor(WHITE);
        v.setTypeface(Typeface.MONOSPACE);v.setPadding(dp(10),dp(7),dp(10),dp(7));return v;
    }
    private Button button(String title,Runnable action){
        Button b=new Button(activity);b.setText(title);b.setTextColor(WHITE);b.setAllCaps(false);
        b.setTypeface(Typeface.MONOSPACE);b.setTextSize(14);
        b.setBackgroundTintList(ColorStateList.valueOf(0xff0d548f));
        b.setOnClickListener(v->action.run());return b;
    }
    private void choice(LinearLayout parent,String title,int index,String[] options){
        parent.addView(button(title+": "+options[values[index]],()->
            new AlertDialog.Builder(activity).setTitle(title).setSingleChoiceItems(options,values[index],(d,n)->{
                values[index]=n;changed();d.dismiss();showMenu();
            }).setNegativeButton("Volver",null).show()));
    }
    private void slider(LinearLayout parent,String name,int index,int minimum,int maximum){
        TextView text=label(name+": "+values[index]+"%",14);parent.addView(text);
        SeekBar seek=new SeekBar(activity);seek.setMax(maximum-minimum);seek.setProgress(values[index]-minimum);
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onStartTrackingTouch(SeekBar s){}
            public void onStopTrackingTouch(SeekBar s){changed();}
            public void onProgressChanged(SeekBar s,int n,boolean user){if(user){values[index]=n+minimum;text.setText(name+": "+values[index]+"%");}}
        });parent.addView(seek);
    }

    private void open(){
        if(dialog!=null&&dialog.isShowing())return;
        send(true);dialog=new Dialog(activity);dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setOnDismissListener(d->{if(editingBefore!=null){values=editingBefore;editingBefore=null;}persist();send(false);dialog=null;});showMenu();dialog.show();
        Window w=dialog.getWindow();if(w!=null){w.setLayout(-1,-1);w.setBackgroundDrawableResource(android.R.color.transparent);
            w.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            if(Build.VERSION.SDK_INT>=28){WindowManager.LayoutParams a=w.getAttributes();a.layoutInDisplayCutoutMode=1;w.setAttributes(a);}
            w.getDecorView().setSystemUiVisibility(5894);
        }
    }

    private void showMenu(){
        LinearLayout panel=new LinearLayout(activity);panel.setOrientation(LinearLayout.VERTICAL);panel.setBackgroundColor(BLUE);
        panel.setPadding(dp(16),dp(8),dp(16),dp(12));
        TextView title=label("SONIC ADVANCE 2 · AJUSTES",20);title.setTextColor(GOLD);panel.addView(title);
        panel.addView(button("Continuar jugando",()->dialog.dismiss()));
        choice(panel,"Pantalla",0,new String[]{"Proporción correcta","Píxeles exactos"});
        choice(panel,"Filtro",1,new String[]{"Nítido · vecino más cercano","Suave"});
        panel.addView(label("Los menús conservan 3:2; la escena ampliada conserva su proporción.",12));
        panel.addView(button("Editar posición y tamaño de cada control",()->showEditor()));
        slider(panel,"Tamaño de controles",SIZE,55,140);slider(panel,"Opacidad",OPACITY,10,100);
        slider(panel,"Área táctil",HIT,100,160);
        choice(panel,"Controles visibles",8,new String[]{"Automático con mando","Siempre","Ocultos"});
        slider(panel,"Volumen general",5,0,100);slider(panel,"Música",6,0,100);slider(panel,"Efectos",7,0,100);
        panel.addView(button("Presentación: "+values[9]+" FPS",()->{values[9]=values[9]==60?30:60;changed();showMenu();}));
        panel.addView(label("La lógica del juego continúa a 60 Hz en ambos modos.",12));
        panel.addView(button("Diagnóstico FPS: "+(values[10]==0?"desactivado":"activado"),()->{values[10]^=1;changed();showMenu();}));
        panel.addView(button("Créditos y atribuciones",()->new AlertDialog.Builder(activity).setTitle("Créditos")
            .setMessage("Sonic Advance 2: SEGA / Sonic Team / Dimps.\nDescompilación y port portátil: SAT-R, Jace, Ollie y colaboradores del proyecto sa2.\nSDL: Sam Lantinga y colaboradores, licencia zlib.\nAndroid y personajes personalizados: YlPorts.\nSans pertenece a Toby Fox; Spider-Man a Marvel.\n\nLas atribuciones completas y licencias permanecen en el código fuente y README del repositorio YlPorts/sa2.")
            .setPositiveButton("Volver",null).show()));
        panel.addView(button("Restaurar ajustes",()->new AlertDialog.Builder(activity).setMessage("¿Restaurar los ajustes? El progreso del juego se conserva.")
            .setPositiveButton("Restaurar",(d,n)->{values=defaults();changed();showMenu();}).setNegativeButton("Cancelar",null).show()));
        ScrollView scroll=new ScrollView(activity);scroll.addView(panel);dialog.setContentView(scroll);
    }

    private void showEditor(){
        int[] before=values.clone();editingBefore=before;FrameLayout root=new FrameLayout(activity);root.setBackgroundColor(0xee052546);
        ControlEditor canvas=new ControlEditor();root.addView(canvas,new FrameLayout.LayoutParams(-1,-1));
        LinearLayout tools=new LinearLayout(activity);tools.setOrientation(LinearLayout.HORIZONTAL);
        tools.addView(button("Guardar",()->{editingBefore=null;changed();showMenu();}));
        tools.addView(button("Cancelar",()->{values=before;editingBefore=null;showMenu();}));
        tools.addView(button("Restaurar",()->{for(int i=LAYOUT;i<COUNT;i++)values[i]=defaults()[i];canvas.invalidate();}));
        tools.addView(label("Arrastra · Toca dos veces para cambiar tamaño",12));
        FrameLayout.LayoutParams bar=new FrameLayout.LayoutParams(-1,-2,Gravity.TOP);root.addView(tools,bar);
        dialog.setContentView(root);
    }

    private final class ControlEditor extends View {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        private int selected=-1;private float offsetX,offsetY;private long lastDown;
        ControlEditor(){super(activity);setContentDescription("Editor de controles: arrastra cada botón");}
        private float[] rect(int id){
            float w=getWidth(),h=getHeight(),b=Math.min(w,h),u=b*.105f;
            float[][] base={{b*.22f,h-b*.25f,u*1.5f,u*1.5f},{w-b*.19f,h-b*.295f,b*.082f,b*.082f},
                {w-b*.355f,h-b*.17f,b*.073f,b*.073f},{b*.17f,b*.22f,b*.105f,b*.05f},
                {w-b*.17f,b*.22f,b*.105f,b*.05f},{w*.5f-b*.10f,h-b*.07f,b*.07f,b*.038f},
                {w*.5f+b*.10f,h-b*.07f,b*.07f,b*.038f}};
            float[] r=base[id];int j=LAYOUT+id*3;float scale=values[SIZE]*values[j+2]/10000f;
            r[2]*=scale;r[3]*=scale;if(values[j]>=0)r[0]=w*values[j]/1000f;if(values[j+1]>=0)r[1]=h*values[j+1]/1000f;
            r[0]=Math.max(r[2],Math.min(w-r[2],r[0]));r[1]=Math.max(r[3],Math.min(h-r[3],r[1]));return r;
        }
        @Override protected void onDraw(Canvas c){
            paint.setTypeface(Typeface.MONOSPACE);paint.setTextAlign(Paint.Align.CENTER);
            for(int i=0;i<7;i++){float[] r=rect(i);paint.setColor(i==selected?GOLD:0xffb7d0f7);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(dp(2));
                c.drawRoundRect(r[0]-r[2],r[1]-r[3],r[0]+r[2],r[1]+r[3],r[3],r[3],paint);
                paint.setStyle(Paint.Style.FILL);paint.setTextSize(Math.max(dp(10),r[3]*.65f));c.drawText(LABELS[i],r[0],r[1]+r[3]*.22f,paint);
                paint.setColor(0x559bd5ff);paint.setStyle(Paint.Style.STROKE);float hit=values[HIT]/100f;
                c.drawOval(r[0]-r[2]*hit,r[1]-r[3]*hit,r[0]+r[2]*hit,r[1]+r[3]*hit,paint);paint.setStyle(Paint.Style.FILL);
            }
        }
        @Override public boolean onTouchEvent(MotionEvent e){
            if(e.getActionMasked()==MotionEvent.ACTION_DOWN){
                int old=selected;selected=-1;float best=Float.MAX_VALUE;
                for(int i=0;i<7;i++){float[] r=rect(i);float dx=(e.getX()-r[0])/r[2],dy=(e.getY()-r[1])/r[3],d=dx*dx+dy*dy;
                    if(d<2.5f&&d<best){best=d;selected=i;}}
                if(selected>=0){float[] r=rect(selected);offsetX=e.getX()-r[0];offsetY=e.getY()-r[1];
                    if(old==selected&&e.getEventTime()-lastDown<320){int j=LAYOUT+selected*3+2;
                        final int index=j;new AlertDialog.Builder(activity).setTitle("Tamaño: "+LABELS[selected])
                        .setItems(new String[]{"Pequeño · 75%","Normal · 100%","Grande · 130%"},(d,n)->{values[index]=new int[]{75,100,130}[n];invalidate();}).show();}
                    lastDown=e.getEventTime();}invalidate();return true;
            }
            if(e.getActionMasked()==MotionEvent.ACTION_MOVE&&selected>=0){int j=LAYOUT+selected*3;
                values[j]=Math.max(0,Math.min(1000,Math.round((e.getX()-offsetX)*1000/getWidth())));
                values[j+1]=Math.max(0,Math.min(1000,Math.round((e.getY()-offsetY)*1000/getHeight())));invalidate();return true;}
            if(e.getActionMasked()==MotionEvent.ACTION_UP){performClick();return true;}return true;
        }
        @Override public boolean performClick(){super.performClick();return true;}
    }
}
