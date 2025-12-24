import { Injectable } from '@angular/core';
import { initializeApp } from 'firebase/app';
import { getDatabase, ref, onValue, query, limitToLast } from 'firebase/database';
import { environment } from '../../../environments/environment';
import { BehaviorSubject } from 'rxjs';

export interface HiveLatest {
  ts: string;
  weight: number;
  temp: number;
  hum: number;
  light: number;
}

export interface HiveReading extends HiveLatest {}

@Injectable({ providedIn: 'root' })
export class RtdbService {
  private app = initializeApp(environment.firebase);
  private db = getDatabase(this.app);

  private latestSubject = new BehaviorSubject<HiveLatest | null>(null);
  latest$ = this.latestSubject.asObservable();

  private readingsSubject = new BehaviorSubject<HiveReading[]>([]);
  readings$ = this.readingsSubject.asObservable();

  start(deviceId: string, chartLimit = 50): void {
    const latestRef = ref(this.db, `devices/${deviceId}/latest`);
    onValue(latestRef, (snap) => {
      const v = snap.val();
      if (!v) return;
      this.latestSubject.next({
        ts: v.ts ?? '',
        weight: Number(v.weight ?? 0),
        temp: Number(v.temp ?? 0),
        hum: Number(v.hum ?? 0),
        light: Number(v.light ?? 0),
      });
    });

    const readingsQ = query(ref(this.db, `devices/${deviceId}/readings`), limitToLast(chartLimit));
    onValue(readingsQ, (snap) => {
      const obj = snap.val() || {};
      const arr: HiveReading[] = Object.values(obj).map((x: any) => ({
        ts: x.ts ?? '',
        weight: Number(x.weight ?? 0),
        temp: Number(x.temp ?? 0),
        hum: Number(x.hum ?? 0),
        light: Number(x.light ?? 0),
      }));

      // Sort po "YYYY-MM-DD HH:mm:ss" timestampu
      arr.sort((a, b) => (a.ts || '').localeCompare(b.ts || ''));
      this.readingsSubject.next(arr);
    });
  }
}
