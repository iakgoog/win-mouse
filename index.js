"use strict";
const EventEmitter = require('events');
const bindings = require('bindings');
const Mouse = bindings('addon').Mouse;
class WinMouseEvent extends EventEmitter {
    constructor() {
        super();
        this.left = false;
        this.right = false;
        this.initNewListener.bind(this);
        this.once('newListener', this.initNewListener);
    }
    initNewListener() {
        this.mouse = new Mouse((type, x, y) => {
            if (type === 'left-down')
                this.left = true;
            else if (type === 'left-up')
                this.left = false;
            else if (type === 'right-down')
                this.right = true;
            else if (type === 'right-up')
                this.right = false;
            if (type === 'move' && this.left)
                type = 'left-drag';
            else if (type === 'move' && this.right)
                type = 'right-drag';
            this.emit(type, x, y);
        });
    }
    ref() {
        this.mouse && this.mouse.ref();
    }
    unref() {
        this.mouse && this.mouse.unref();
    }
    destroy() {
        this.mouse && this.mouse.destroy();
        this.mouse = null;
    }
}
const winMouseEvent = new WinMouseEvent();
module.exports = winMouseEvent;
