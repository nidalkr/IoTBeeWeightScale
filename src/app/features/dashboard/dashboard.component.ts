import { Component, OnInit, OnDestroy } from '@angular/core';
import { CommonModule } from '@angular/common';
import { BaseChartDirective } from 'ng2-charts';
import { RtdbService } from '../../core/services/rtdb.service';
import { Observable } from 'rxjs';
import { HiveLatest, HiveReading } from '../../core/services/rtdb.service';
import { ChartConfiguration } from 'chart.js';

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [CommonModule, BaseChartDirective],
  templateUrl: './dashboard.component.html',
  styleUrl: './dashboard.component.scss',
})
export class DashboardComponent implements OnInit, OnDestroy {

  deviceId = 'beehive1';

  latest$!: Observable<HiveLatest | null>;
  readings$!: Observable<HiveReading[]>;

  statusLabel: 'Normal' | 'Alert' = 'Normal';

  // ✅ Connection status based on last update freshness
  connectionStatus: 'Online' | 'Offline' = 'Offline';
  private offlineThresholdMs = 90_000; // 90s
  private lastTs: string | null = null;
  private timerId: any;

  // ✅ Weight trend
  private prevWeight: number | null = null;
  weightTrend: 'increase' | 'decrease' | 'stable' = 'stable';

  lineChartData: ChartConfiguration<'line'>['data'] = {
    labels: [],
    datasets: [{ data: [], label: 'Weight (kg)', tension: 0.35, pointRadius: 4 }],
  };

  lineChartOptions: ChartConfiguration<'line'>['options'] = {
    responsive: true,
    animation: false,
    plugins: { legend: { display: false } },
  };

  constructor(private rtdb: RtdbService) {}

  ngOnInit(): void {
    // start listening
    this.rtdb.start(this.deviceId, 50);

    this.latest$ = this.rtdb.latest$;
    this.readings$ = this.rtdb.readings$;

    this.latest$.subscribe(v => {
      if (!v) return;

      this.lastTs = v.ts;

      // status badge example
      this.statusLabel = v.weight < 1 ? 'Alert' : 'Normal';

      // ✅ weight trend vs previous latest
      if (this.prevWeight !== null) {
        const diff = v.weight - this.prevWeight;
        const eps = 0.05; // 50g prag (podesi)
        if (diff > eps) this.weightTrend = 'increase';
        else if (diff < -eps) this.weightTrend = 'decrease';
        else this.weightTrend = 'stable';
      }
      this.prevWeight = v.weight;

      // update connection status immediately
      this.updateConnectionStatus(v.ts);
    });

    this.readings$.subscribe(items => {
      this.lineChartData = {
        labels: items.map(x => (x.ts || '').slice(11, 16)),
        datasets: [{
          data: items.map(x => x.weight),
          label: 'Weight (kg)',
          tension: 0.35,
          pointRadius: 4
        }]
      };
    });

    // periodic check: if ESP stops sending, status becomes Offline
    this.timerId = setInterval(() => {
      if (!this.lastTs) {
        this.connectionStatus = 'Offline';
        return;
      }
      this.updateConnectionStatus(this.lastTs);
    }, 5_000);
  }

  ngOnDestroy(): void {
    if (this.timerId) clearInterval(this.timerId);
  }

  // =====================
  // Connection helpers
  // =====================
  private updateConnectionStatus(ts: string): void {
    const last = this.parseTs(ts);
    if (!last) {
      this.connectionStatus = 'Offline';
      return;
    }

    const ageMs = Date.now() - last.getTime();
    this.connectionStatus = ageMs <= this.offlineThresholdMs ? 'Online' : 'Offline';
  }

  // expected ts format: "YYYY-MM-DD HH:mm:ss"
  private parseTs(ts: string): Date | null {
    if (!ts) return null;
    const iso = ts.replace(' ', 'T'); // "2025-12-20T16:30:00"
    const d = new Date(iso);
    return isNaN(d.getTime()) ? null : d;
  }

  // =====================
  // UI helpers
  // =====================
  lightText(light: number): string {
    if (light > 750) return 'Sunny';
    if (light > 350) return 'Cloudy';
    return 'Night';
  }

  weightTrendLabel(): string {
    if (this.weightTrend === 'increase') return 'Increasing';
    if (this.weightTrend === 'decrease') return 'Decreasing';
    return 'Stable';
  }

  weightTrendIcon(): string {
    if (this.weightTrend === 'increase') return 'bi-graph-up';
    if (this.weightTrend === 'decrease') return 'bi-graph-down';
    return 'bi-dash-lg';
  }

  weightTrendClass(): string {
    if (this.weightTrend === 'increase') return 'text-success';
    if (this.weightTrend === 'decrease') return 'text-danger';
    return 'text-muted';
  }

  // Temperature status (podesi pragove po potrebi)
  tempStatus(t: number | null | undefined): { label: string; className: string } {
    if (t === null || t === undefined || Number.isNaN(t)) {
      return { label: '— No data', className: 'text-muted' };
    }

    if (t < 5) return { label: '— Low', className: 'text-info' };
    if (t >= 18) return { label: '— Optimal', className: 'text-success' };
    if (t >= 30) return { label: '— High', className: 'text-warning' };
    return { label: '— Critical', className: 'text-danger' };
  }

  // Humidity status (podesi pragove po potrebi)
  humStatus(h: number | null | undefined): { label: string; className: string } {
    if (h === null || h === undefined || Number.isNaN(h)) {
      return { label: '— No data', className: 'text-muted' };
    }

    if (h < 45) return { label: '— Dry', className: 'text-warning' };
    if (h <= 65) return { label: '— Normal', className: 'text-success' };
    if (h <= 75) return { label: '— Humid', className: 'text-warning' };
    return { label: '— Very humid', className: 'text-danger' };
  }
}
