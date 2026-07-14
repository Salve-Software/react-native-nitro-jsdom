/** The request passed to `onFetch` when `fetch(url, init)` is called inside the sandbox. */
export interface IFetchRequestInit {
  method: string;
  headers: Record<string, string>;
  body?: string;
}

/** The response `onFetch` must resolve with. */
export interface IFetchResponseInit {
  status: number;
  statusText?: string;
  headers?: Record<string, string>;
  body?: string;
}
