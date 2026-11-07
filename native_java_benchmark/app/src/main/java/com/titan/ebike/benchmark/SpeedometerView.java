package com.titan.ebike.benchmark;

import android.content.Context;
import android.graphics.*;
import android.util.AttributeSet;
import android.view.View;

/**
 * Direct Canvas-backed Speedometer running with Hardware Acceleration
 * for Native Android performance benchmarking against Flutter.
 */
public class SpeedometerView extends View {
    private final Paint trackPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint progressPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private float speed = 0;
    private final float maxSpeed = 120.0f;
    private final RectF arcRect = new RectF();

    public SpeedometerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        trackPaint.setStyle(Paint.Style.STROKE);
        trackPaint.setStrokeWidth(28f);
        trackPaint.setColor(Color.parseColor("#1E293B"));
        trackPaint.setStrokeCap(Paint.Cap.ROUND);

        progressPaint.setStyle(Paint.Style.STROKE);
        progressPaint.setStrokeWidth(28f);
        progressPaint.setColor(Color.parseColor("#06B6D4"));
        progressPaint.setStrokeCap(Paint.Cap.ROUND);

        textPaint.setColor(Color.WHITE);
        textPaint.setTextSize(92f);
        textPaint.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.BOLD));
        textPaint.setTextAlign(Paint.Align.CENTER);

        labelPaint.setColor(Color.parseColor("#06B6D4"));
        labelPaint.setTextSize(24f);
        labelPaint.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.BOLD));
        labelPaint.setTextAlign(Paint.Align.CENTER);
    }

    public void setSpeed(float speed) {
        this.speed = speed;
        invalidate(); // Request redraw
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float pad = 40f;
        arcRect.set(pad, pad, w - pad, h - pad);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float startAngle = 135f;
        float sweepAngle = 270f;

        canvas.drawArc(arcRect, startAngle, sweepAngle, false, trackPaint);
        float progressSweep = (Math.min(speed, maxSpeed) / maxSpeed) * sweepAngle;
        canvas.drawArc(arcRect, startAngle, progressSweep, false, progressPaint);

        float cx = arcRect.centerX();
        float cy = arcRect.centerY();
        canvas.drawText(String.valueOf((int) speed), cx, cy + 20f, textPaint);
        canvas.drawText("KM/H", cx, cy + 65f, labelPaint);
    }
}