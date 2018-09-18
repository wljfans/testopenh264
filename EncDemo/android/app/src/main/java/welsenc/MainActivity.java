//package encoder.test.alivc.aliyun.com.encodertest;
package welsenc;

import android.Manifest;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.PermissionChecker;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;


public class MainActivity extends AppCompatActivity
{
    private int mNoPermissionIndex = 0;
    private final int PERMISSION_REQUEST_CODE = 1;
    private int permission_count = 0;
    private  String info;
    private Button bn;
    private final String[] permissionManifest = {
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE
    };

    private final String[] noPermissionTip = {
            "写权限",
            "读权限"
    };
    TextView view;
    Handler myHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case 0x0000:
                    view.setText("running...");
                    break;
                case 0x0001:
                    view.setText("done");
                    break;
                case 0x0002:
                    String str = "error: " + info;
                    view.setText(str);
                    break;
                case 0x0003:

                    if(permission_count < 2)
                    {

                    }
                    else
                    {
                        bn.setClickable(false);
                        new Thread(){
                            @Override
                            public void run() {
                                super.run();
                                Log.e("test","start");
                                myHandler.sendEmptyMessage(0x0000);
                                int ret = WelsEncTest.encoderTest();
                                bn.setClickable(true);
                                Log.e("test","end");
                                if(ret == 0)
                                    myHandler.sendEmptyMessage(0x0001);
                                else {
                                    if(ret == -1)
                                    {
                                        info = "参数错误";
                                    }
                                    else if(ret == -2)
                                    {
                                        info = "打开文件失败";
                                    }
                                    else if(ret == -3)
                                    {
                                        info = "解码器创建失败";
                                    }
                                    else
                                    {
                                        info = "位置错误";
                                    }
                                    myHandler.sendEmptyMessage(0x0002);
                                }

                            }
                        }.start();
                    }

            }
            super.handleMessage(msg);
        }
    };
    /**
     * 权限检查（适配6.0以上手机）
     */
    private boolean permissionCheck() {
        int permissionCheck = PackageManager.PERMISSION_GRANTED;
        String permission;
        for (int i = 0; i < permissionManifest.length; i++) {
            permission = permissionManifest[i];
            mNoPermissionIndex = i;
            if (PermissionChecker.checkSelfPermission(this, permission)
                    != PackageManager.PERMISSION_GRANTED) {
                permissionCheck = PackageManager.PERMISSION_DENIED;
            }
        }
        if (permissionCheck != PackageManager.PERMISSION_GRANTED) {
            return false;
        } else {
            return true;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        view = (TextView)findViewById(R.id.testview);
        bn = (Button)findViewById(R.id.bn);
        bn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                myHandler.sendEmptyMessage(0x0003);
            }
        });
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        if (!permissionCheck()) {
            if (Build.VERSION.SDK_INT >= 23) {
                ActivityCompat.requestPermissions(this, permissionManifest, PERMISSION_REQUEST_CODE);
            } else {
                showNoPermissionTip(noPermissionTip[mNoPermissionIndex]);
                finish();
            }
        }
        else
        {
            permission_count = 2;
        }
    }

    private void showNoPermissionTip(String tip) {
        Toast.makeText(this, tip, Toast.LENGTH_LONG).show();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        switch (requestCode) {
            case PERMISSION_REQUEST_CODE:
                for (int i = 0; i < permissions.length; i++) {
                    if (grantResults[i] == PackageManager.PERMISSION_DENIED) {
                        String toastTip = noPermissionTip[i];
                        mNoPermissionIndex = i;
                        if (toastTip.length() > 0) {
                            ToastUtils.showToast(MainActivity.this, toastTip);
                            finish();
                        }
                    }
                    permission_count++;
                    if(permission_count >= 2)
                    {
                        myHandler.sendEmptyMessage(0x0003);
                    }
                }
                break;
        }

    }


    static {
        System.loadLibrary("alivch264enc");
        System.loadLibrary("enc_auto_test");
    }
}

