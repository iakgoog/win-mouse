declare const EventEmitter: any;
declare const bindings: any;
declare const Mouse: any;
declare class WinMouseEvent extends EventEmitter {
    private mouse;
    private left;
    private right;
    constructor();
    initNewListener(): void;
    ref(): void;
    unref(): void;
    destroy(): void;
}
declare const winMouseEvent: WinMouseEvent;

export = winMouseEvent;
