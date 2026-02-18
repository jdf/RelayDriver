/**
 * @file main.c
 * @author jdf
 * @date 2026-02-16
 * @brief Listen for a rising edge on PB2 (INT0) and toggle a relay connected to
 * PB0/PB1 accordingly.
 */

#ifndef F_CPU
#define F_CPU 1000000UL // 1 MHz (default ATtiny85 internal oscillator)
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdbool.h>

// Relay control pins
#define RELAY_SET PB0
#define RELAY_RESET PB1

// Pulse duration: 100ms at 1MHz with prescaler 64 = ~156 Timer0 overflows
// (1000000 / 64 / 256) * 0.1 = 61, use 62 for margin
#define CLOCKS_PER_RELAY_PULSE 62

// Timer0 overflow at ~16.384ms (1MHz/64/256). 16 overflows ~= 0.262s.
#define WDT_TICK_COUNT 16

#define TIMER0_PRESCALE_BITS ((1 << CS01) | (1 << CS00)) // Timer0 prescaler 64
#define TIMER0_INTERRUPT_ENABLE_MASK (1 << TOIE0)

// Timer1 prescaler 64 (CS13..CS10 = 0b0111 on ATtiny85 Timer1)
#define TIMER1_PRESCALE_BITS ((1 << CS12) | (1 << CS11) | (1 << CS10))
#define TIMER1_INTERRUPT_ENABLE_MASK (1 << TOIE1)

volatile uint8_t debounce_and_relay_counter = 0;
volatile uint8_t active_pin_out = RELAY_RESET;
volatile uint8_t wdt_tick_counter = 0;

// Start Timer0 with prescaler 64
void start_debounce_timer() {
  TCNT0 = 0; // Reset counter so overflow timing is consistent
  TCCR0B |= TIMER0_PRESCALE_BITS;
  TIMSK |= TIMER0_INTERRUPT_ENABLE_MASK; // Enable Timer0 OVF interrupt
}

// Stop Timer0
void stop_debounce_timer() {
  TCCR0B &= ~TIMER0_PRESCALE_BITS;
  TIMSK &= ~TIMER0_INTERRUPT_ENABLE_MASK; // Disable Timer0 OVF interrupt
}

// Start Timer1 for watchdog check-ins
void start_watchdog_timer() {
  TCNT1 = 0;
  TCCR1 |= TIMER1_PRESCALE_BITS;
  TIMSK |= TIMER1_INTERRUPT_ENABLE_MASK; // Enable Timer1 OVF interrupt
}

volatile bool service_switch_interrupt = false;
volatile bool service_debounce_timer_interrupt = false;
volatile bool service_watchdog_timer_interrupt = false;

ISR(INT0_vect) { service_switch_interrupt = true; }
ISR(TIM0_OVF_vect) { service_debounce_timer_interrupt = true; }
ISR(TIM1_OVF_vect) { service_watchdog_timer_interrupt = true; }

// Interrupt 0 is triggered on a rising edge of PB2, so we can use it to toggle
// the relay and start a debounce timer. The debounce timer will keep the relay
// pin high for a short duration to ensure the relay is activated, and then
// clear the pin after the pulse duration has elapsed.
void enable_int0() {
  // Rising edge = 0b11 for ISC01..ISC00
  MCUCR |= (1 << ISC01) | (1 << ISC00);
  GIMSK |= (1 << INT0); // Enable INT0
}

void disable_int0() { GIMSK &= ~(1 << INT0); }

void service_switch() {
  if (active_pin_out == RELAY_SET) {
    active_pin_out = RELAY_RESET;
  } else {
    active_pin_out = RELAY_SET;
  }

  // debounce
  disable_int0();

  // Set relay pin high and start timer.
  PORTB |= (1 << active_pin_out);
  debounce_and_relay_counter = CLOCKS_PER_RELAY_PULSE;
  start_debounce_timer();
}

void service_debounce_timer() {
  if (debounce_and_relay_counter > 0) {
    debounce_and_relay_counter--;
  } else {
    // Debounce and relay pulse duration has elapsed, clear relay pin and
    // re-enable INT0 for the next switch event.
    PORTB &= ~(1 << active_pin_out);
    stop_debounce_timer();
    enable_int0();
  }
}

void service_watchdog_timer() {
  wdt_tick_counter++;
  if (wdt_tick_counter >= WDT_TICK_COUNT) {
    wdt_reset();
    wdt_tick_counter = 0;
    // Reset Timer1 counter to maintain consistent overflow timing
    TCNT1 = 0;
  }
}

int main() {
  // Configure relay control pins as outputs
  DDRB |= (1 << RELAY_SET) | (1 << RELAY_RESET);
  // Initialize both relay pins low
  PORTB &= ~((1 << RELAY_SET) | (1 << RELAY_RESET));

  // Configure INT0 (PB2) for rising edge interrupt
  MCUCR |= (1 << ISC01) | (1 << ISC00);
  GIMSK |= (1 << INT0); // Enable INT0

  // Set sleep mode to idle (lowest power consumption while keeping timers
  // running)
  set_sleep_mode(SLEEP_MODE_IDLE);

  // Start Timer1 for periodic watchdog check-ins
  start_watchdog_timer();

  // Enable watchdog reset at 1 second
  wdt_enable(WDTO_1S);
  wdt_reset();

  // Enable interrupts globally
  sei();

  while (1) {
    service_switch_interrupt = false;
    service_debounce_timer_interrupt = false;
    service_watchdog_timer_interrupt = false;
    sleep_mode();
    if (service_switch_interrupt) {
      service_switch();
    }
    if (service_debounce_timer_interrupt) {
      service_debounce_timer();
    }
    if (service_watchdog_timer_interrupt) {
      service_watchdog_timer();
    }
  }

  return 0;
}
