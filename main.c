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

// Timer0 prescaler 64
#define TIMER0_PRESCALE_BITS ((1 << CS01) | (1 << CS00))
#define TIMER0_INTERRUPT_ENABLE_MASK (1 << TOIE0)

// Timer1 prescaler 64 (CS13..CS10 = 0b0111 on ATtiny85 Timer1)
#define TIMER1_PRESCALE_BITS ((1 << CS12) | (1 << CS11) | (1 << CS10))
#define TIMER1_INTERRUPT_ENABLE_MASK (1 << TOIE1)

volatile uint8_t debounce_and_relay_counter = 0;
volatile uint8_t active_pin_out = RELAY_RESET;
volatile uint8_t wdt_tick_counter = 0;

volatile bool pet_the_dog = false;

void start_debounce_timer() {
  TCNT0 = 0;
  TCCR0B |= TIMER0_PRESCALE_BITS;
  TIMSK |= TIMER0_INTERRUPT_ENABLE_MASK; // Enable Timer0 OVF interrupt
}

void stop_debounce_timer() {
  TCCR0B &= ~TIMER0_PRESCALE_BITS;
  TIMSK &= ~TIMER0_INTERRUPT_ENABLE_MASK; // Disable Timer0 OVF interrupt
}

void start_watchdog_timer() {
  TCNT1 = 0;
  TCCR1 |= TIMER1_PRESCALE_BITS;
  TIMSK |= TIMER1_INTERRUPT_ENABLE_MASK; // Enable Timer1 OVF interrupt
}

// Interrupt 0 is triggered on a rising edge of PB2.
void enable_switch_interrupt() {
  // Rising edge = 0b11 for ISC01..ISC00
  MCUCR |= (1 << ISC01) | (1 << ISC00);
  GIMSK |= (1 << INT0); // Enable INT0
}

void disable_switch_interrupt() { GIMSK &= ~(1 << INT0); }

// Service switch going high.
ISR(INT0_vect) {
  if (active_pin_out == RELAY_SET) {
    active_pin_out = RELAY_RESET;
  } else {
    active_pin_out = RELAY_SET;
  }

  // debounce
  disable_switch_interrupt();

  // Set relay pin high and start timer.
  PORTB |= (1 << active_pin_out);
  debounce_and_relay_counter = CLOCKS_PER_RELAY_PULSE;
  start_debounce_timer();
}

// Service debounce timer.
ISR(TIM0_OVF_vect) {
  if (debounce_and_relay_counter > 0) {
    debounce_and_relay_counter--;
  } else {
    // Debounce and relay pulse duration has elapsed, clear relay pin and
    // re-enable INT0 for the next switch event.
    PORTB &= ~(1 << active_pin_out);
    stop_debounce_timer();
    enable_switch_interrupt();
  }
}

// Service watchdog timer.
ISR(TIM1_OVF_vect) {
  wdt_tick_counter++;
  if (wdt_tick_counter >= WDT_TICK_COUNT) {
    wdt_tick_counter = 0;
    TCNT1 = 0;
    pet_the_dog = true;
  }
}

int main() {
  // Configure relay control pins as outputs
  DDRB |= (1 << RELAY_SET) | (1 << RELAY_RESET);
  // Initialize both relay pins low
  PORTB &= ~((1 << RELAY_SET) | (1 << RELAY_RESET));

  enable_switch_interrupt();
  start_watchdog_timer();

  wdt_enable(WDTO_1S); // Enable watchdog reset at 1 second
  wdt_reset();

  set_sleep_mode(SLEEP_MODE_IDLE); // allow timers during sleep
  sei();                           // enable interrupts globally

  while (1) {
    sleep_mode();
    if (pet_the_dog) {
      pet_the_dog = false;
      wdt_reset();
    }
  }

  return 0;
}
