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
  private offlineThresholdMs = 90_000; // 90s (podesi po potrebi)
  private lastTs: string | null = null;
  private timerId: any;

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

  lightText(light: number): string {
    if (light > 750) return 'Sunny';
    if (light > 350) return 'Cloudy';
    return 'Night';
  }
}
