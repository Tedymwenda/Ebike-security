package com.titan.ebike.benchmark;

import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import java.util.EnumMap;

public class MainActivity extends AppCompatActivity {
    private enum SecurityTarget {
        FARDRIVER("FARDRIVER", "far_driver"),
        SMART_BMS("SMART BMS", "smart_bms"),
        IGNITION("IGNITION", "ignition");

        private final String label;
        private final String routeId;

        SecurityTarget(String label, String routeId) {
            this.label = label;
            this.routeId = routeId;
        }
    }

    private static final class RouteBinding {
        final String routeId;
        final String hardwareAddress;

        RouteBinding(String routeId, String hardwareAddress) {
            this.routeId = routeId;
            this.hardwareAddress = hardwareAddress;
        }
    }

    private static final EnumMap<SecurityTarget, RouteBinding> ROUTES = new EnumMap<>(SecurityTarget.class);

    static {
        ROUTES.put(SecurityTarget.FARDRIVER, new RouteBinding(SecurityTarget.FARDRIVER.routeId, null));
        ROUTES.put(SecurityTarget.SMART_BMS, new RouteBinding(SecurityTarget.SMART_BMS.routeId, null));
        ROUTES.put(SecurityTarget.IGNITION, new RouteBinding(SecurityTarget.IGNITION.routeId, null));
    }

    private SecurityTarget activeTarget = SecurityTarget.FARDRIVER;
    private ScrollView root;
    private LinearLayout content;
    private LinearLayout headerRow;
    private LinearLayout headerTitle;
    private LinearLayout panelRoute;
    private LinearLayout panelAccess;
    private LinearLayout targetRow;
    private TextView title;
    private TextView subtitle;
    private TextView badge;
    private TextView routeLabel;
    private TextView routeValue;
    private TextView componentLabel;
    private TextView activeTargetLabel;
    private TextView credentialLabel;
    private TextView result;
    private TextView notice;
    private EditText credentialInput;
    private Button farDriverButton;
    private Button smartBmsButton;
    private Button ignitionButton;
    private Button submitButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().setStatusBarColor(Color.rgb(3, 7, 18));
        getWindow().setNavigationBarColor(Color.rgb(3, 7, 18));

        root = findViewById(R.id.root);
        content = findViewById(R.id.content);
        headerRow = findViewById(R.id.header_row);
        headerTitle = findViewById(R.id.header_title);
        panelRoute = findViewById(R.id.panel_route);
        panelAccess = findViewById(R.id.panel_access);
        targetRow = findViewById(R.id.target_row);
        title = findViewById(R.id.tv_title);
        subtitle = findViewById(R.id.tv_subtitle);
        badge = findViewById(R.id.tv_badge);
        routeLabel = findViewById(R.id.tv_route_label);
        routeValue = findViewById(R.id.tv_route_value);
        componentLabel = findViewById(R.id.tv_component_label);
        activeTargetLabel = findViewById(R.id.tv_active_target);
        credentialLabel = findViewById(R.id.tv_credential_label);
        result = findViewById(R.id.tv_result);
        notice = findViewById(R.id.tv_notice);
        credentialInput = findViewById(R.id.et_credential);
        farDriverButton = findViewById(R.id.btn_far_driver);
        smartBmsButton = findViewById(R.id.btn_smart_bms);
        ignitionButton = findViewById(R.id.btn_ignition);
        submitButton = findViewById(R.id.btn_submit);

        applyPanelStyle(panelRoute);
        applyPanelStyle(panelAccess);
        badge.setBackground(roundRect(0xFF0F172A, 0xFF334155, 8));
        credentialInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        credentialInput.setBackground(roundRect(0xFF030712, 0xFF334155, 8));
        submitButton.setBackground(roundRect(0xFF06B6D4, 0xFF06B6D4, 8));
        submitButton.setTextColor(Color.rgb(3, 7, 18));

        farDriverButton.setOnClickListener(v -> selectTarget(SecurityTarget.FARDRIVER));
        smartBmsButton.setOnClickListener(v -> selectTarget(SecurityTarget.SMART_BMS));
        ignitionButton.setOnClickListener(v -> selectTarget(SecurityTarget.IGNITION));
        submitButton.setOnClickListener(v -> submitCredential());
        selectTarget(SecurityTarget.FARDRIVER);
    }

    private void selectTarget(SecurityTarget target) {
        activeTarget = target;
        credentialInput.setText("");
        activeTargetLabel.setText("ACTIVE TARGET: " + target.label);
        result.setText("Select a component and enter its credential.");
        styleTargetButton(farDriverButton, target == SecurityTarget.FARDRIVER);
        styleTargetButton(smartBmsButton, target == SecurityTarget.SMART_BMS);
        styleTargetButton(ignitionButton, target == SecurityTarget.IGNITION);
    }

    private void submitCredential() {
        String enteredCredential = credentialInput.getText().toString().trim();
        if (enteredCredential.isEmpty()) {
            result.setText("Enter a password or PIN for " + activeTarget.label + ".");
            return;
        }

        RouteBinding route = ROUTES.get(activeTarget);
        credentialInput.setText("");
        if (route == null || route.hardwareAddress == null) {
            result.setText("ROUTE BLOCKED: No hardware address is configured for "
                    + activeTarget.label + ". No BLE data was sent.");
            return;
        }

        result.setText("ROUTE BLOCKED: Credential transport is not configured for " + route.routeId + ".");
    }

    private void applyPanelStyle(View view) {
        view.setBackground(roundRect(0xFF0F172A, 0xFF1E293B, 12));
    }

    private void styleTargetButton(Button button, boolean selected) {
        int fill = selected ? 0xFF06B6D4 : 0xFF030712;
        int stroke = selected ? 0xFF06B6D4 : 0xFF334155;
        button.setBackground(roundRect(fill, stroke, 8));
        button.setTextColor(selected ? Color.rgb(3, 7, 18) : Color.rgb(203, 213, 225));
    }

    private GradientDrawable roundRect(int fillColor, int strokeColor, int radiusDp) {
        float density = getResources().getDisplayMetrics().density;
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(fillColor);
        drawable.setCornerRadius(radiusDp * density);
        drawable.setStroke(Math.max(1, Math.round(density)), strokeColor);
        return drawable;
    }
}